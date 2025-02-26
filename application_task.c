/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2024, Northern Mechatronics, Inc.
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
#include <string.h>

#include <am_mcu_apollo.h>
#include <am_util.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "am_bsp.h"

#include "button.h"
#include "led.h"
#include "lorawan.h"

#include "gpio_cli.h"

#include "application_task.h"
#include "application_task_cli.h"

#if defined(BSP_NM180410) || defined(BSP_NM180411)
    #define APPLICATION_LED AM_BSP_GPIO_LED0
    #define APPLICATION_LED_TIMER_NUMBER    (1)
    #define APPLICATION_LED_TIMER_SEGMENT   AM_HAL_CTIMER_TIMERA
    #define APPLICATION_LED_TIMER_INTERRUPT AM_HAL_CTIMER_INT_TIMERA1C0
#else
    #define APPLICATION_LED AM_BSP_GPIO_LED1
    #define APPLICATION_LED_TIMER_NUMBER    (2)
    #define APPLICATION_LED_TIMER_SEGMENT   AM_HAL_CTIMER_TIMERB
    #define APPLICATION_LED_TIMER_INTERRUPT AM_HAL_CTIMER_INT_TIMERB2C0
#endif

#define APPLICATION_DEFAULT_LORAWAN_CLASS LORAWAN_CLASS_A
#define APPLICATION_QUEUE_MAX_SIZE        (10)
#define APPLICATION_RX_BUFFER_MAX_SIZE    (256)

#define LORAWAN_PM_ENABLE   1

typedef enum
{
    APP_MSG_RX,
    APP_MSG_BUTTON_PRESSED,
} application_message_t;

typedef struct
{
    uint32_t counter;
    uint32_t port;
    int32_t slot;
    int32_t dr;
    int32_t rssi;
    int32_t snr;
    uint32_t size;
    uint8_t *payload;
} application_rx_packet_t;

static uint8_t application_rx_buffer[APPLICATION_RX_BUFFER_MAX_SIZE];
static application_rx_packet_t application_rx_packet;
static volatile uint32_t lorawan_joining;

static TaskHandle_t application_task_handle;
static QueueHandle_t application_queue_handle;
static uint32_t application_led_handle;

static void on_lorawan_join_request(LmHandlerJoinParams_t *params)
{
    // Reset the joining flag.  It will be set
    // in the MLME Request callback.
    lorawan_joining = 0;
    if (params->Status == LORAMAC_HANDLER_ERROR)
    {
        lorawan_join();
    }
    else
    {
        lorawan_class_set(APPLICATION_DEFAULT_LORAWAN_CLASS);
        lorawan_request_time_sync();

        led_command_t command = {
            .ui32Handle = application_led_handle,
            .ui32Id = LED_EFFECT_PULSE2,
            .ui32Repeat = 1
        };
        led_send(&command);
    }
}

static void on_lorawan_receive(LmHandlerAppData_t *data, LmHandlerRxParams_t *params)
{
    application_message_t msg;

    // Class B beacon messages has no data
    if (data == NULL)
    {
        return;
    }

    // Port 0 is reserved for MAC layer messages
    if (data->Port == 0)
    {
        return;
    }

    application_rx_packet.counter = params->DownlinkCounter;
    application_rx_packet.port = data->Port;
    application_rx_packet.slot = params->RxSlot;
    application_rx_packet.dr = params->Datarate;
    application_rx_packet.rssi = params->Rssi;
    application_rx_packet.snr = params->Snr;
    application_rx_packet.size = data->BufferSize;
    application_rx_packet.payload = application_rx_buffer;
    memcpy(application_rx_buffer, data->Buffer, data->BufferSize);

    // This is executed within the LoRaWAN transport layer context,
    // do not block.  In this example, we timeout in 100ms if the queue is full.
    msg = APP_MSG_RX;
    xQueueSend(application_queue_handle, &msg, pdMS_TO_TICKS(100));
}

static void on_lorawan_mlme_request(LoRaMacStatus_t status, MlmeReq_t *mlme, TimerTime_t delay)
{
    if (mlme->Type == MLME_JOIN)
    {
        if (status == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED)
        {
            led_command_t command = {
                .ui32Handle = application_led_handle,
                .ui32Id = LED_EFFECT_PULSE1,
                .ui32Repeat = 1
            };
            led_send(&command);
            lorawan_joining = 0;
        }
        else
        {
            led_command_t command = {
                .ui32Handle = application_led_handle,
                .ui32Id = LED_EFFECT_BREATHING,
                .ui32Repeat = 0
            };
            led_send(&command);
            lorawan_joining = 1;
        }
    }
}

static void on_lorawan_sleep(void)
{
#if defined(BSP_NM180410) || defined(BSP_NM180411)
    if (!lorawan_joining)
    {
        am_hal_gpio_state_write(AM_BSP_GPIO_PETAL_CORE_nLORA_EN, AM_HAL_GPIO_OUTPUT_SET);
    }
#endif
}

static void on_lorawan_wake(void)
{
#if defined(BSP_NM180410) || defined(BSP_NM180411)
    am_hal_gpio_state_write(AM_BSP_GPIO_PETAL_CORE_nLORA_EN, AM_HAL_GPIO_OUTPUT_CLEAR);
#endif
}

static void on_button_pressed(void)
{
    // This callback executes in the button task context and not from an ISR.
    application_message_t msg = APP_MSG_BUTTON_PRESSED;
    xQueueSend(application_queue_handle, &msg, pdMS_TO_TICKS(100));
}

static void on_led_ctimer(void)
{
    led_interrupt_service(application_led_handle);
}

static void process_downlink_packet(void)
{
    am_util_stdio_printf("\n\rReceived Data\n\r");
    am_util_stdio_printf("  COUNTER   : %-4d\n\r", application_rx_packet.counter);
    am_util_stdio_printf("  PORT      : %-4d\n\r", application_rx_packet.port);
    am_util_stdio_printf("  SLOT      : %-4d\n\r", application_rx_packet.slot);
    am_util_stdio_printf("  DATA RATE : %-4d\n\r", application_rx_packet.dr);
    am_util_stdio_printf("  RSSI      : %-4d\n\r", application_rx_packet.rssi);
    am_util_stdio_printf("  SNR       : %-4d\n\r", application_rx_packet.snr);
    am_util_stdio_printf("  SIZE      : %-4d\n\r", application_rx_packet.size);
    am_util_stdio_printf("  PAYLOAD   :");
    for (int i = 0; i < application_rx_packet.size; i++)
    {
        if ((i % 8) == 0)
        {
            am_util_stdio_printf("\n\r    ");
        }
        am_util_stdio_printf("%02x ", application_rx_packet.payload[i]);
    }
    am_util_stdio_printf("\n\r");

    // blink the LED to indicate receive
    led_command_t command = {
        .ui32Handle = application_led_handle,
        .ui32Id = LED_EFFECT_PULSE2,
        .ui32Repeat = 1,
    };
    led_send(&command);
}

static void process_button_press(void)
{
    // Transmit a single byte to indicate to the LNS that a button
    // sequence has been pressed:
    //   Port: 1
    //   ACK:  0  (0 for no acknowledgment, 1 for acknowledgment)
    //   Payload Size: 1 byte
    //   Payload: "B"
    lorawan_transmit(1, 0, 1, "B");
}

static void setup_button(void)
{
    uint32_t handle;
    button_config(&handle, AM_BSP_GPIO_BUTTON0, g_AM_BSP_GPIO_BUTTON0, 1);
    
    // The following register a single short press sequence to the button.
    // Other press sequences are possible.  For example, a two presses sequence
    // with a short press first followed by a long press would be
    // 
    // button_sequence_register(handle, 2, 0b10, on_button_pressed);
    //
    // In general, the order of the sequence starts from LSB to MSB;
    // 0 being a short press and 1 being a long press.
    button_sequence_register(handle, 1, 0b0, on_button_pressed);
}

static void setup_led(void)
{
    // Configure the LED that is connected to a GPIO with CTIMER output.
    const led_config_t led_cfg = {
        .ui32Number    = APPLICATION_LED_TIMER_NUMBER,
        .ui32Segment   = APPLICATION_LED_TIMER_SEGMENT,
        .ui32Interrupt = APPLICATION_LED_TIMER_INTERRUPT,
        .ui32ActiveLow    = 0,
        .ui32Pin       = APPLICATION_LED,
        .pfnInterruptService = on_led_ctimer,
    };

    led_config(&application_led_handle, &led_cfg);
}

static void setup_lorawan(void)
{
    lorawan_joining = 0;
    lorawan_tracing_set(1);

    lorawan_network_config(LORAWAN_REGION_US915, LORAWAN_DATARATE_0, true, true);

    lorawan_activation_config(LORAWAN_ACTIVATION_OTAA, NULL);

    lorawan_key_set_by_str(LORAWAN_KEY_JOIN_EUI, "b4c231a359bc2e3d");
    lorawan_key_set_by_str(LORAWAN_KEY_APP, "01c3f004a2d6efffe32c4eda14bcd2b4");
    lorawan_key_set_by_str(LORAWAN_KEY_NWK, "3f4ca100e2fc675ea123f4eb12c4a012");

    // While callbacks are executed outside of interrupt context, they are
    // called by the LoRaWAN stack transport layer.  Avoid blocking or
    // performing excessive processing in these callbacks.
    lorawan_event_callback_register(LORAWAN_EVENT_MAC_MLME_REQUEST,
                                    on_lorawan_mlme_request);
    lorawan_event_callback_register(LORAWAN_EVENT_JOIN_REQUEST,
                                    on_lorawan_join_request);
    lorawan_event_callback_register(LORAWAN_EVENT_RX_DATA,
                                    on_lorawan_receive);
    
#if (LORAWAN_PM_ENABLE == 1)
    lorawan_event_callback_register(LORAWAN_EVENT_SLEEP,
                                    on_lorawan_sleep);
    lorawan_event_callback_register(LORAWAN_EVENT_WAKE,
                                    on_lorawan_wake);
#endif

    // Start the LoRaWAN stack.
    // There is no need to explicitly turn on the radio,
    // radio power management is handled by the stack.
    lorawan_stack_state_set(LORAWAN_STACK_STARTED);

    // In the event of a reboot, check the join context in flash.
    // If the device had already joined the network, switch to the
    // default LoRaWAN class.
    if (lorawan_get_join_state())
    {
        lorawan_class_set(APPLICATION_DEFAULT_LORAWAN_CLASS);
    }
}

static void application_task_setup(void)
{
    am_hal_gpio_pinconfig(AM_BSP_GPIO_LED0, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_LED0, AM_HAL_GPIO_OUTPUT_CLEAR);

    am_hal_gpio_pinconfig(AM_BSP_GPIO_LED1, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_LED1, AM_HAL_GPIO_OUTPUT_CLEAR);

    am_hal_gpio_pinconfig(AM_BSP_GPIO_LED2, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_LED2, AM_HAL_GPIO_OUTPUT_CLEAR);

    am_hal_gpio_pinconfig(AM_BSP_GPIO_LED3, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_LED3, AM_HAL_GPIO_OUTPUT_CLEAR);

    am_hal_gpio_pinconfig(AM_BSP_GPIO_LED4, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_LED4, AM_HAL_GPIO_OUTPUT_CLEAR);

// The Petal ecosystem has the ability to shutdown the LoRa radio.  In addition,
// the Petal development board has the ability to shutdown the I/O level shifters
// for power savings.
#if defined(BSP_NM180410) || defined(BSP_NM180411)
    am_hal_gpio_pinconfig(AM_BSP_GPIO_PETAL_CORE_nLORA_EN, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_PETAL_CORE_nLORA_EN, AM_HAL_GPIO_OUTPUT_SET);

    am_hal_gpio_pinconfig(AM_BSP_GPIO_PETAL_DEV_IO_EN, g_AM_HAL_GPIO_OUTPUT);
    am_hal_gpio_state_write(AM_BSP_GPIO_PETAL_DEV_IO_EN, AM_HAL_GPIO_OUTPUT_SET);
#endif

    setup_button();
    setup_led();
    setup_lorawan();
}

static void application_task_loop(void)
{
    application_message_t msg;

    // Wait for 1s and then toggle the LED.  Change the delay to portMAX_DELAY in
    // production for power savings if LED indicator is not used:
    //
    //   xQueueReceive(application_queue_handle, &msg, portMAX_DELAY)
    //
    if (xQueueReceive(application_queue_handle, &msg, pdMS_TO_TICKS(1000)) == pdPASS)
    {
        switch (msg)
        {
        case APP_MSG_RX:
            process_downlink_packet();
            break;

        case APP_MSG_BUTTON_PRESSED:
            process_button_press();
            break;

        default:
            break;
        }
    }

    if (led_status_get(application_led_handle) == LED_STATUS_IDLE)
    {
        am_hal_gpio_state_write(AM_BSP_GPIO_LED1, AM_HAL_GPIO_OUTPUT_TOGGLE);
    }
}

static void application_task(void *parameter)
{
#if defined(CLI_ENABLE)
    gpio_cli_register();
    application_task_cli_register();
#endif
    application_task_setup();
    while (1)
    {
        application_task_loop();
    }
}

void application_task_create(uint32_t priority)
{
    application_queue_handle =
        xQueueCreate(APPLICATION_QUEUE_MAX_SIZE, sizeof(application_message_t));
    xTaskCreate(application_task, "application", 512, 0, priority, &application_task_handle);
}
