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
#include <list.h>
#include <task.h>
#include <queue.h>
#include <timers.h>

#include "am_bsp.h"
#include "button.h"
#include "button_task.h"

#define BUTTON_HANDLE_MAX           (4)

#define BUTTON_DEBOUNCE_DELAY_MS   (20)
#define BUTTON_PRESS_SAMPLING_MS   (20)
#define BUTTON_PRESS_GAP_MS       (500)
#define BUTTON_PRESS_SHORT_MS     (500)
#define BUTTON_PRESS_LONG_MS     (2000)

typedef enum
{
    BUTTON_PRESS_SHORT = 0,
    BUTTON_PRESS_LONG = 1,
} button_press_e;

typedef enum
{
    BUTTON_STATE_TIMEOUT,
    BUTTON_STATE_PRESSED,
} button_command_e;

typedef struct
{
    button_command_e command;
    uint32_t         handle;
} button_msg_t;

typedef struct
{
    uint8_t size:8;
    uint32_t sequence:24;
} button_sequence_t;

typedef union
{
    button_sequence_t sequence;
    uint32_t          value;
} button_sequence_u;

typedef struct {
    uint32_t             pin;
    uint32_t             active_low;
    am_hal_gpio_pincfg_t config;
    TimerHandle_t        timer;
    List_t               sequence;
} button_cfg_t;

static button_cfg_t button_cfg[BUTTON_HANDLE_MAX];

static uint32_t button_state_counter[BUTTON_HANDLE_MAX];
static uint32_t pressed_sequence[BUTTON_HANDLE_MAX];

static uint32_t button_config_max;

static TaskHandle_t  button_task_handle;
static QueueHandle_t button_queue_handle;

static void button_task_send_command(button_command_e command, uint32_t handle)
{
    BaseType_t xHigherPriorityTaskWoken;
    button_msg_t msg = { command, handle };

    if (xPortIsInsideInterrupt() == pdTRUE)
    {
        xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(button_queue_handle, &msg, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    else
    {
        xQueueSend(button_queue_handle, &msg, portMAX_DELAY);
    }
}

static void button_task_timer_callback(TimerHandle_t xTimer)
{
    button_task_send_command(BUTTON_STATE_TIMEOUT, (uint32_t) pvTimerGetTimerID(xTimer));
}

static void button_press_handler(void *context)
{
    uint32_t handle = (uint32_t)context;

    AM_HAL_GPIO_MASKCREATE(pin_interrupt);
    AM_HAL_GPIO_MASKBITSMULT(ppin_interrupt, button_cfg[handle].pin);

    am_hal_gpio_interrupt_disable(ppin_interrupt);
    am_hal_gpio_interrupt_clear(ppin_interrupt);

    button_task_send_command(BUTTON_STATE_PRESSED, handle);
}

static button_press_e button_press_classification(uint32_t handle)
{
    uint32_t initial_state;
    uint32_t current_state;
    uint32_t duration;

    AM_HAL_GPIO_MASKCREATE(pin_interrupt);
    AM_HAL_GPIO_MASKBITSMULT(ppin_interrupt, button_cfg[handle].pin);

    xTimerStop(button_cfg[handle].timer, portMAX_DELAY);

    vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_DELAY_MS));
    am_hal_gpio_state_read(button_cfg[handle].pin, AM_HAL_GPIO_INPUT_READ, &initial_state);
    if (initial_state == button_cfg[handle].active_low)
    {
        am_hal_gpio_interrupt_clear(ppin_interrupt);
        am_hal_gpio_interrupt_enable(ppin_interrupt);

        return BUTTON_PRESS_SHORT;
    }

    duration = BUTTON_DEBOUNCE_DELAY_MS;
    current_state = initial_state;
    while (current_state == initial_state)
    {
        vTaskDelay(pdMS_TO_TICKS(BUTTON_PRESS_SAMPLING_MS));
        am_hal_gpio_state_read(button_cfg[handle].pin, AM_HAL_GPIO_INPUT_READ, &current_state);
        duration += BUTTON_PRESS_SAMPLING_MS;
    }

    am_hal_gpio_interrupt_clear(ppin_interrupt);
    am_hal_gpio_interrupt_enable(ppin_interrupt);

    xTimerChangePeriod(button_cfg[handle].timer, pdMS_TO_TICKS(BUTTON_PRESS_GAP_MS), portMAX_DELAY);

    if (duration < BUTTON_PRESS_SHORT_MS)
    {
        return BUTTON_PRESS_SHORT;
    }

    return BUTTON_PRESS_LONG;
}

static void button_sequence_set_bit(uint32_t *pSequence, uint32_t count, button_press_e press)
{
    *pSequence |= (press << count);
}

static void button_sequence_execute(uint32_t handle)
{
    ListItem_t *pItem = listGET_HEAD_ENTRY(&button_cfg[handle].sequence);

    while (pItem != (ListItem_t *)&(button_cfg[handle].sequence.xListEnd))
    {
        button_sequence_u seq = (button_sequence_u)pItem->xItemValue;
        if ((button_state_counter[handle] == seq.sequence.size)
         && (pressed_sequence[handle] == seq.sequence.sequence))
        {
            sequence_callback_t callback = (sequence_callback_t)pItem->pvOwner;
            if (callback != NULL)
            {
                callback();
                pItem = listGET_NEXT(pItem);
                continue;
            }
        }
        pItem = listGET_NEXT(pItem);
    }
}

static void button_task(void *parameter)
{
    button_msg_t msg;
    button_press_e press_type;

    for (;;)
    {
        if (xQueueReceive(button_queue_handle, &msg, portMAX_DELAY) == pdPASS)
        {
            switch(msg.command)
            {
            case BUTTON_STATE_PRESSED:
                press_type = button_press_classification(msg.handle);
                button_sequence_set_bit(&pressed_sequence[msg.handle], button_state_counter[msg.handle], press_type);
                button_state_counter[msg.handle]++;
                break;

            case BUTTON_STATE_TIMEOUT:
                button_sequence_execute(msg.handle);
                button_state_counter[msg.handle] = 0;
                pressed_sequence[msg.handle] = 0;
                break;

            default:
                button_state_counter[msg.handle] = 0;
                pressed_sequence[msg.handle] = 0;
                break;
            }
        }
     }
}

void button_task_create(uint32_t priority)
{
    button_config_max = 0;
    button_queue_handle = xQueueCreate(16, sizeof(button_msg_t));

    for (uint32_t handle = 0; handle < BUTTON_HANDLE_MAX; ++handle)
    {
        button_state_counter[handle] = 0;
        pressed_sequence[handle] = 0;
    }

    xTaskCreate(button_task, "Button Task", 512, 0, priority, &button_task_handle);
}

void button_config(uint32_t *pui32Handle, uint32_t ui32Pin, am_hal_gpio_pincfg_t sConfig, uint32_t ui32ActiveLow)
{
    if (button_config_max >= BUTTON_HANDLE_MAX)
    {
        return;
    }

    *pui32Handle = button_config_max;

    button_cfg[*pui32Handle].pin = ui32Pin;    
    button_cfg[*pui32Handle].active_low = ui32ActiveLow ? 1 : 0;    
    memcpy(&button_cfg[*pui32Handle].config, &sConfig, sizeof(am_hal_gpio_pincfg_t));
    button_cfg[*pui32Handle].timer = xTimerCreate("Button Timer", pdMS_TO_TICKS(1000), pdFALSE, (void *)(*pui32Handle), button_task_timer_callback);

    AM_HAL_GPIO_MASKCREATE(pin_interrupt);
    AM_HAL_GPIO_MASKBITSMULT(ppin_interrupt, ui32Pin);

    am_hal_gpio_pinconfig(button_cfg[*pui32Handle].pin, button_cfg[*pui32Handle].config);
    am_hal_gpio_interrupt_register_adv(button_cfg[*pui32Handle].pin, button_press_handler, (void *)(*pui32Handle));
    am_hal_gpio_interrupt_clear(ppin_interrupt);
    am_hal_gpio_interrupt_enable(ppin_interrupt);
    NVIC_EnableIRQ(GPIO_IRQn);

    vListInitialise(&button_cfg[*pui32Handle].sequence);

    button_config_max++;
}

void button_sequence_register(uint32_t ui32Handle, uint32_t ui32Size, uint32_t ui32Value, sequence_callback_t pfnCallback)
{
    ListItem_t *pItem = pvPortMalloc(sizeof(ListItem_t));
    vListInitialiseItem(pItem);
    vListInsert(&button_cfg[ui32Handle].sequence, pItem);

    button_sequence_u seq;
    seq.sequence.size     = ui32Size;
    seq.sequence.sequence = ui32Value;

    pItem->xItemValue = seq.value;
    pItem->pvOwner = pfnCallback;
}

void button_sequence_unregister(uint32_t ui32Handle, uint32_t ui32Size, uint32_t ui32Value, sequence_callback_t pfnCallback)
{
    ListItem_t *pItem = listGET_HEAD_ENTRY(&button_cfg[ui32Handle].sequence);

    while (pItem != (ListItem_t *)&(button_cfg[ui32Handle].sequence.xListEnd))
    {
        button_sequence_u seq = (button_sequence_u)pItem->xItemValue;
        if ((ui32Size == seq.sequence.size) && (ui32Value == seq.sequence.sequence))
        {
            sequence_callback_t callback = (sequence_callback_t)pItem->pvOwner;
            if (callback == pfnCallback)
            {
                uxListRemove(pItem);
                vPortFree(pItem);
                return;
            }
        }
        pItem = listGET_NEXT(pItem);
    }
}