/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2022, Northern Mechatronics, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <am_bsp.h>
#include <am_mcu_apollo.h>
#include <am_util.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <timers.h>

#include <LmHandler.h>
#include <LmhpClockSync.h>
#include <LmhpCompliance.h>
#include <LmhpFragmentation.h>
#include <LmhpRemoteMcastSetup.h>
#include <board.h>
#include <radio.h>

#include "lorawan.h"
#include "lorawan_config.h"

#include "lorawan_task.h"
#include "lorawan_task_cli.h"

extern void *SX126xHandle;
extern CommissioningParams_t CommissioningParams;
extern uint32_t LmAbpLrWanVersion;

volatile lorawan_stack_state_e lorawan_stack_state;
uint32_t lorawan_tracing_enabled;

#define LORAWAN_SPI_PORT_TIMEOUT 8000
#define LM_BUFFER_SIZE           242
static uint8_t psLmDataBuffer[LM_BUFFER_SIZE];

typedef struct
{
    LmHandlerMsgTypes_t tType;
    uint32_t ui32Port;
    uint32_t ui32Length;
    uint8_t *pui8Data;
} lorawan_tx_packet_t;

static uint32_t radio_port_powered;
static uint32_t lorawan_mac_pending;

static TaskHandle_t lorawan_task_handle;
static QueueHandle_t command_queue;
static QueueHandle_t transmit_queue;
static TimerHandle_t radio_port_timer;

static LmHandlerParams_t lmh_parameters;
static LmHandlerCallbacks_t lmh_callbacks;
static LmhpFragmentationParams_t lmhp_fragmentation_parameters;

static void radio_port_shutdown(TimerHandle_t timer)
{
    if (radio_port_powered)
    {
        am_hal_iom_power_ctrl(SX126xHandle, AM_HAL_SYSCTRL_DEEPSLEEP, true);
        radio_port_powered = false;
    }
}

static void lorawan_task_on_sleep()
{
    if (Radio.GetStatus() == RF_IDLE)
    {
        typedef void (*callback_t)(void);
        callback_t callback = (callback_t)lorawan_event_callback_list[LORAWAN_EVENT_SLEEP];
        if (callback)
        {
            callback();
        }

        xTimerChangePeriod(radio_port_timer, pdMS_TO_TICKS(LORAWAN_SPI_PORT_TIMEOUT), 0);
    }
}

static void lorawan_task_on_wake()
{
    xTimerStop(radio_port_timer, 0);

    // turn on the radio immediately as the stack may require access to the radio
    // hardware upon wake
    typedef void (*callback_t)(void);
    callback_t callback = (callback_t)lorawan_event_callback_list[LORAWAN_EVENT_WAKE];
    if (callback)
    {
        callback();
    }

    if (radio_port_powered == false)
    {
        am_hal_iom_power_ctrl(SX126xHandle, AM_HAL_SYSCTRL_WAKE, true);
        radio_port_powered = true;
    }

    // The radio looses the public network value across power cycles.
    // The value can be retrieved from the MIB.  An MIB Set will
    // trigger a write to the public network register in the radio.
    MibRequestConfirm_t mibReq;
    mibReq.Type = MIB_PUBLIC_NETWORK;
    LoRaMacMibGetRequestConfirm(&mibReq);
    LoRaMacMibSetRequestConfirm(&mibReq);
}

void lorawan_task_wake()
{
    BaseType_t xHigherPriorityTaskWoken;

    lorawan_mac_pending = 1;

    // we power up the radio here as the LoRaWAN stack performs chip access
    // within an IRQ.
    typedef void (*callback_t)(void);
    callback_t callback = (callback_t)lorawan_event_callback_list[LORAWAN_EVENT_WAKE];
    if (callback)
    {
        callback();
    }

    if (radio_port_powered == false)
    {
        am_hal_iom_power_ctrl(SX126xHandle, AM_HAL_SYSCTRL_WAKE, true);
        radio_port_powered = true;
    }

    if (lorawan_task_handle == NULL)
    {
        return;
    }

    if (xPortIsInsideInterrupt() == pdTRUE)
    {
        xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(lorawan_task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    else
    {
        xTaskNotifyGive(lorawan_task_handle);
    }
}

void lorawan_wake_on_radio_irq()
{
    lorawan_task_wake();
}

void lorawan_wake_on_timer_irq()
{
    lorawan_task_wake();
}

static void on_mac_process_notify()
{
    lorawan_mac_pending = 1;
}

static void lorawan_task_handle_command()
{
    lorawan_command_t command;

    // do not block on message receive as the LoRa MAC state machine decides
    // when it is appropriate to sleep.  We also do not explicitly go to
    // sleep directly and simply do a task yield.  This allows other timing
    // critical radios such as BLE to run their state machines.
    if (xQueueReceive(command_queue, &command, 0) == pdPASS)
    {
        if (command.eCommand == LORAWAN_START)
        {
            lorawan_stack_state_set(LORAWAN_STACK_STARTED);
            return;
        }

        if (lorawan_stack_state == LORAWAN_STACK_STARTED)
        {
            switch (command.eCommand)
            {
            case LORAWAN_STOP:
                lorawan_stack_state_set(LORAWAN_STACK_STOPPED);
                break;
            case LORAWAN_JOIN:
                LmHandlerJoin();
                break;
            case LORAWAN_SYNC_APP:
                LmhpClockSyncAppTimeReq();
                break;
            case LORAWAN_SYNC_MAC:
                LmHandlerDeviceTimeReq();
                break;
            case LORAWAN_CLASS_SET:
                LmHandlerRequestClass((DeviceClass_t)command.pvParameters);
                break;
            default:
                break;
            }
        }
    }
}

static void lorawan_task_handle_uplink()
{
    lorawan_tx_packet_t packet;
    if (xQueuePeek(transmit_queue, &packet, 0) == pdPASS)
    {
        if (LmHandlerIsBusy() == true)
        {
            return;
        }

        xQueueReceive(transmit_queue, &packet, 0);

        LmHandlerAppData_t app_data;

        if (packet.ui32Length > 0)
        {
            memcpy(psLmDataBuffer, packet.pui8Data, packet.ui32Length);
            vPortFree(packet.pui8Data);
        }
        app_data.Port = packet.ui32Port;
        app_data.BufferSize = packet.ui32Length;
        app_data.Buffer = psLmDataBuffer;

        // The LoRaWAN spec does not prevent the user from 
        // transmitting during a multicast session.  However,
        // doing so will impact downlink messages from the LNS
        // during a multicast.  We default to dequeue and return
        // if we are in a multicast session.
        if (LmhpRemoteMcastSessionStateStarted())
        {
            return;
        }

        LmHandlerSend(&app_data, packet.tType);
    }
}

static void lorawan_task(void *pvParameters)
{
    lorawan_mac_pending = 0;
    lorawan_stack_state = LORAWAN_STACK_STOPPED;
    lorawan_task_cli_register();

    while (1)
    {
        if (lorawan_stack_state == LORAWAN_STACK_STARTED)
        {
            LmHandlerProcess();
            lorawan_task_handle_uplink();
        }

        lorawan_task_handle_command();

        if (lorawan_mac_pending)
        {
            lorawan_mac_pending = 0;
        }
        else
        {
            lorawan_task_on_sleep();
            ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
            lorawan_task_on_wake();
        }
    }
}

void lorawan_network_config(lorawan_region_e eRegion,
                            lorawan_datarate_e eDataRate,
                            int8_t i8ADR,
                            int8_t i8PublicNetwork)
{
    LoRaMacRegion_t region;

    switch(eRegion)
    {
    case LORAWAN_REGION_AS923:
        region = LORAMAC_REGION_AS923;
        break;
    case LORAWAN_REGION_AU915:
        region = LORAMAC_REGION_AU915;
        break;
    case LORAWAN_REGION_CN470:
        region = LORAMAC_REGION_CN470;
        break;
    case LORAWAN_REGION_CN779:
        region = LORAMAC_REGION_CN779;
        break;
    case LORAWAN_REGION_EU433:
        region = LORAMAC_REGION_EU433;
        break;
    case LORAWAN_REGION_EU868:
        region = LORAMAC_REGION_EU868;
        break;
    case LORAWAN_REGION_KR920:
        region = LORAMAC_REGION_KR920;
        break;
    case LORAWAN_REGION_IN865:
        region = LORAMAC_REGION_IN865;
        break;
    case LORAWAN_REGION_US915:
        region = LORAMAC_REGION_US915;
        break;
    case LORAWAN_REGION_RU864:
        region = LORAMAC_REGION_RU864;
        break;
    default:
        return;
    }

    lmh_parameters.Region = region;
    lmh_parameters.AdrEnable = i8ADR;
    lmh_parameters.TxDatarate = eDataRate;
    lmh_parameters.PublicNetworkEnable = i8PublicNetwork;
}

void lorawan_activation_config(lorawan_activation_type_e sType,
                               lorawan_activation_parameters_t *psParams)
{
    if (sType == LORAWAN_ACTIVATION_OTAA)
    {
        CommissioningParams.IsOtaaActivation = true;
    }
    else if (sType == LORAWAN_ACTIVATION_ABP)
    {
        CommissioningParams.IsOtaaActivation = false;

        if (psParams != NULL)
        {
            LmAbpLrWanVersion = psParams->abp_server_version;
            CommissioningParams.NetworkId = psParams->abp_network_id;
            CommissioningParams.DevAddr = psParams->abp_device_address;
        }
    }
}

void lorawan_stack_state_set(lorawan_stack_state_e eState)
{
    switch (eState)
    {
    case LORAWAN_STACK_STARTED:
        if (lorawan_stack_state == LORAWAN_STACK_STOPPED)
        {
            lorawan_task_on_wake();
            BoardInitMcu();
            BoardInitPeriph();

            lmh_parameters.DataBufferMaxSize = LM_BUFFER_SIZE;
            lmh_parameters.DataBuffer = psLmDataBuffer;

            switch (lmh_parameters.Region)
            {
            case LORAMAC_REGION_EU868:
            case LORAMAC_REGION_RU864:
            case LORAMAC_REGION_CN779:
                lmh_parameters.DutyCycleEnabled = true;
                break;
            default:
                lmh_parameters.DutyCycleEnabled = false;
                break;
            }

            lmh_callbacks_setup(&lmh_callbacks);
            lmh_callbacks.OnMacProcess = on_mac_process_notify;

            LmHandlerInit(&lmh_callbacks, &lmh_parameters);
            LmHandlerSetSystemMaxRxError(20);

            LmhpComplianceParams_t lmhp_compliance_parameters;
            LmHandlerPackageRegister(PACKAGE_ID_COMPLIANCE, &lmhp_compliance_parameters);
            LmHandlerPackageRegister(PACKAGE_ID_CLOCK_SYNC, NULL);
            LmHandlerPackageRegister(PACKAGE_ID_REMOTE_MCAST_SETUP, NULL);

            lmhp_fragmentation_setup(&lmhp_fragmentation_parameters);
            LmHandlerPackageRegister(PACKAGE_ID_FRAGMENTATION, &lmhp_fragmentation_parameters);

            lorawan_stack_state = LORAWAN_STACK_STARTED;
            if (LmHandlerJoinStatus() == LORAMAC_HANDLER_SET)
            {
                LmHandlerDeviceTimeReq();
            }
            Radio.Sleep();

            radio_port_powered = true;
            lorawan_task_wake();
        }
        break;

    case LORAWAN_STACK_STOPPED:
        if (lorawan_stack_state == LORAWAN_STACK_STARTED)
        {
            LoRaMacStop();
            LoRaMacDeInitialization();
            BoardDeInitMcu();
            lorawan_task_on_sleep();
            xQueueReset(transmit_queue);

            lorawan_stack_state = LORAWAN_STACK_STOPPED;
            radio_port_powered = false;

            lorawan_task_wake();
        }
        break;

    default:
        break;
    }
}

void lorawan_stack_state_get(lorawan_stack_state_e *peState)
{
    *peState = lorawan_stack_state;
}

void lorawan_join()
{
    lorawan_command_t command = { .eCommand = LORAWAN_JOIN, .pvParameters = NULL };
    lorawan_send_command(&command);
}

uint32_t lorawan_get_join_state()
{
    if (LmHandlerJoinStatus() == LORAMAC_HANDLER_SET)
    {
        return 1;
    }

    return 0;
}

void lorawan_class_set(lorawan_class_e eDeviceClass)
{
    DeviceClass_t deviceClass;
    switch (eDeviceClass)
    {
    case LORAWAN_CLASS_A:
        deviceClass = CLASS_A;
        break;
    case LORAWAN_CLASS_B:
        deviceClass = CLASS_B;
        break;
    case LORAWAN_CLASS_C:
        deviceClass = CLASS_C;
        break;
    default:
        return;
    }
    lorawan_command_t command = { .eCommand = LORAWAN_CLASS_SET, .pvParameters = (void *)deviceClass };
    lorawan_send_command(&command);
}

void lorawan_class_get(lorawan_class_e *peDeviceClass)
{
    DeviceClass_t deviceClass = LmHandlerGetCurrentClass();
    switch (deviceClass)
    {
    case CLASS_A:
        *peDeviceClass = LORAWAN_CLASS_A;
        break;
    case CLASS_B:
        *peDeviceClass = LORAWAN_CLASS_B;
        break;
    case CLASS_C:
        *peDeviceClass = LORAWAN_CLASS_C;
        break;
    default:
        return;
    }
}

void lorawan_request_time_sync()
{
    lorawan_command_t command = { .eCommand = LORAWAN_SYNC_MAC, .pvParameters = NULL };
    lorawan_send_command(&command);
}

void lorawan_send_command(lorawan_command_t *psCommand)
{
    xQueueSend(command_queue, psCommand, 0);
    lorawan_task_wake();
}

void lorawan_transmit(uint32_t ui32Port, uint32_t ui32Ack, uint32_t ui32Length, uint8_t *pui8Data)
{
    lorawan_tx_packet_t packet;

    if (LmHandlerJoinStatus() != LORAMAC_HANDLER_SET)
    {
        lorawan_join();
        return;
    }

    packet.tType = ui32Ack ? LORAMAC_HANDLER_CONFIRMED_MSG : LORAMAC_HANDLER_UNCONFIRMED_MSG;
    packet.ui32Port = ui32Port;
    packet.ui32Length = ui32Length;
    packet.pui8Data = NULL;

    if (ui32Length > 0)
    {
        packet.pui8Data = pvPortMalloc(ui32Length);
        memcpy(packet.pui8Data, pui8Data, ui32Length);
    }
    else
    {
        packet.pui8Data = NULL;
    }

    BaseType_t status = xQueueSend(transmit_queue, &packet, 0);
    if (status == pdTRUE)
    {
        lorawan_task_wake();
    }
    else
    {
        if (packet.pui8Data != NULL)
        {
            vPortFree(packet.pui8Data);
        }
    }
}

void lorawan_task_create(uint32_t ui32Priority)
{
    xTaskCreate(lorawan_task, "lorawan", 512, 0, ui32Priority, &lorawan_task_handle);

    command_queue = xQueueCreate(LORAWAN_COMMAND_QUEUE_MAX_SIZE, sizeof(lorawan_command_t));
    transmit_queue = xQueueCreate(LORAWAN_TRANSMIT_QUEUE_MAX_SIZE, sizeof(lorawan_tx_packet_t));

    radio_port_timer = xTimerCreate("LoRaWAN Port Timer",
                                    pdMS_TO_TICKS(LORAWAN_SPI_PORT_TIMEOUT),
                                    pdFALSE,
                                    NULL,
                                    radio_port_shutdown);

    memset(&lmh_callbacks, 0, sizeof(LmHandlerCallbacks_t));
    lorawan_tracing_enabled = 0;
}