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
#include <am_mcu_apollo.h>
#include <am_util.h>

#include <LmHandlerMsgDisplay.h>

#include "lorawan_config.h"

#include "lorawan.h"
#include "lorawan_task.h"

lorawan_event_callback_t lorawan_event_callback_list[LORAWAN_EVENTS];

static void on_mac_process(void)
{
    lorawan_task_wake();

    typedef void (*callback_t)(void);
    callback_t callback = (callback_t)lorawan_event_callback_list[LORAWAN_EVENT_MAC_PROCESS];
    if (callback)
    {
        callback();
    }
}

static void on_nvm_data_change(LmHandlerNvmContextStates_t sState, uint16_t ui16Size)
{
    if (lorawan_tracing_enabled)
    {
        am_util_stdio_printf("\r\n");
        DisplayNvmDataChange(sState, ui16Size);
    }

    typedef void (*callback_t)(LmHandlerNvmContextStates_t, uint16_t);
    callback_t callback = (callback_t)lorawan_event_callback_list[LORAWAN_EVENT_NVM_DATA_CHANGE];
    if (callback)
    {
        callback(sState, ui16Size);
    }
}

static void on_network_parameters_change(CommissioningParams_t *psParams)
{
    if (lorawan_tracing_enabled)
    {
        am_util_stdio_printf("\r\n");
        DisplayNetworkParametersUpdate(psParams);
    }

    typedef void (*callback_t)(CommissioningParams_t *);
    callback_t callback = (callback_t)lorawan_event_callback_list[LORAWAN_EVENT_NETWORK_PARAMETERS_CHANGE];
    if (callback)
    {
        callback(psParams);
    }
}

static void
on_mac_mcps_request(LoRaMacStatus_t eStatus, McpsReq_t *psMcpsReq, TimerTime_t ui32NextTxDelay)
{
    if (lorawan_tracing_enabled)
    {
        am_util_stdio_printf("\r\n");
        DisplayMacMcpsRequestUpdate(eStatus, psMcpsReq, ui32NextTxDelay);
    }

    typedef void (*callback_t)(LoRaMacStatus_t, McpsReq_t *, TimerTime_t);
    callback_t callback = (callback_t)lorawan_event_callback_list[LORAWAN_EVENT_MAC_MCPS_REQUEST];
    if (callback)
    {
        callback(eStatus, psMcpsReq, ui32NextTxDelay);
    }
}

static void
on_mac_mlme_request(LoRaMacStatus_t eStatus, MlmeReq_t *psMlmeReq, TimerTime_t ui32NextTxDelay)
{
    if (lorawan_tracing_enabled)
    {
        am_util_stdio_printf("\r\n");
        DisplayMacMlmeRequestUpdate(eStatus, psMlmeReq, ui32NextTxDelay);
    }

    typedef void (*callback_t)(LoRaMacStatus_t, MlmeReq_t *, TimerTime_t);
    callback_t callback = (callback_t)lorawan_event_callback_list[LORAWAN_EVENT_MAC_MLME_REQUEST];
    if (callback)
    {
        callback(eStatus, psMlmeReq, ui32NextTxDelay);
    }
}

static void on_join_request(LmHandlerJoinParams_t *psParams)
{
    if (lorawan_tracing_enabled)
    {
        am_util_stdio_printf("\r\n");
        DisplayJoinRequestUpdate(psParams);
    }

    typedef void (*callback_t)(LmHandlerJoinParams_t *);
    callback_t callback = (callback_t)lorawan_event_callback_list[LORAWAN_EVENT_JOIN_REQUEST];
    if (callback)
    {
        callback(psParams);
    }
}

static void on_tx_data(LmHandlerTxParams_t *psParams)
{
    if (lorawan_tracing_enabled)
    {
        am_util_stdio_printf("\r\n");
        DisplayTxUpdate(psParams);
    }

    typedef void (*callback_t)(LmHandlerTxParams_t *);
    callback_t callback = (callback_t)lorawan_event_callback_list[LORAWAN_EVENT_TX_DATA];
    if (callback)
    {
        callback(psParams);
    }
}

static void on_rx_data(LmHandlerAppData_t *psAppData, LmHandlerRxParams_t *psParams)
{
    if (lorawan_tracing_enabled)
    {
        am_util_stdio_printf("\r\n");
        DisplayRxUpdate(psAppData, psParams);
    }

    typedef void (*callback_t)(LmHandlerAppData_t *, LmHandlerRxParams_t *);
    callback_t callback = (callback_t)lorawan_event_callback_list[LORAWAN_EVENT_RX_DATA];
    if (callback)
    {
        callback(psAppData, psParams);
    }
}

static void on_class_change(DeviceClass_t eDeviceClass)
{
    if (lorawan_tracing_enabled)
    {
        DisplayClassUpdate(eDeviceClass);
    }

    typedef void (*callback_t)(DeviceClass_t);
    callback_t callback = (callback_t)lorawan_event_callback_list[LORAWAN_EVENT_CLASS_CHANGE];
    if (callback)
    {
        callback(eDeviceClass);
    }
}

static void on_beacon_status_change(LoRaMacHandlerBeaconParams_t *psParams)
{
    if (lorawan_tracing_enabled)
    {
        DisplayBeaconUpdate(psParams);
    }

    typedef void (*callback_t)(LoRaMacHandlerBeaconParams_t *);
    callback_t callback = (callback_t)lorawan_event_callback_list[LORAWAN_EVENT_BEACON_STATUS_CHANGE];
    if (callback)
    {
        callback(psParams);
    }
}

static void on_sys_time_update(bool bSynchronized, int32_t ui32TimeCorrection)
{
    if (lorawan_tracing_enabled)
    {
        am_util_stdio_printf("\r\n");
        am_util_stdio_printf("Clock Synchronized: %d\r\n", bSynchronized);
        am_util_stdio_printf("Correction: %d\r\n", ui32TimeCorrection);
        am_util_stdio_printf("\r\n");
    }

    typedef void (*callback_t)(bool, int32_t);
    callback_t callback = (callback_t)lorawan_event_callback_list[LORAWAN_EVENT_SYS_TIME_UPDATE];
    if (callback)
    {
        callback(bSynchronized, ui32TimeCorrection);
    }
}

void lmh_callbacks_setup(LmHandlerCallbacks_t *psCallbacks)
{
    psCallbacks->GetBatteryLevel = NULL;
    psCallbacks->GetTemperature = NULL;
    psCallbacks->GetRandomSeed = NULL;
    psCallbacks->OnMacProcess = on_mac_process;
    psCallbacks->OnNvmDataChange = on_nvm_data_change;
    psCallbacks->OnNetworkParametersChange = on_network_parameters_change;
    psCallbacks->OnMacMcpsRequest = on_mac_mcps_request;
    psCallbacks->OnMacMlmeRequest = on_mac_mlme_request;
    psCallbacks->OnJoinRequest = on_join_request;
    psCallbacks->OnTxData = on_tx_data;
    psCallbacks->OnRxData = on_rx_data;
    psCallbacks->OnClassChange = on_class_change;
    psCallbacks->OnBeaconStatusChange = on_beacon_status_change;
    psCallbacks->OnSysTimeUpdate = on_sys_time_update;
}

void lorawan_event_callback_register(lorawan_event_e eEvent, lorawan_event_callback_t pfnHandler)
{
    lorawan_event_callback_list[eEvent] = pfnHandler;
}

void lorawan_event_callback_unregister(lorawan_event_e eEvent)
{
    lorawan_event_callback_list[eEvent] = NULL;
}

void lorawan_tracing_set(uint32_t ui32Enabled)
{
    lorawan_tracing_enabled = ui32Enabled;
}