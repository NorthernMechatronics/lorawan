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

#include <am_bsp.h>
#include <am_mcu_apollo.h>
#include <am_util.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <timers.h>

#include "led.h"
#include "led_task.h"
#include "led_task_cli.h"
#include "led_predefined_effects.h"

static TaskHandle_t led_task_handle;
static QueueHandle_t led_task_queue;

uint32_t led_num_count = 0;
uint32_t led_effect_id[LED_EFFECT_MAX];
static uint32_t led_effect_count;
static const led_sequence_t* led_sequence_table[LED_EFFECT_MAX];

typedef struct {
    uint32_t ui32Handle;
    uint32_t effect;
    led_status_t status;
    led_config_t config;
    const led_sequence_t* sequence;
    uint32_t repeat;
    uint32_t count;
    uint32_t index;
} led_context_t;

static led_context_t led_context[LED_NUM_MAX];

static void led_config_duty_cycle(uint32_t ui32Handle, uint32_t ui32OnTime)
{
    uint32_t duty_cycle = ui32OnTime;

    if (ui32OnTime == 0)
    {
        // full off
        am_hal_gpio_pinconfig(led_context[ui32Handle].config.ui32Pin, g_AM_HAL_GPIO_OUTPUT_8);
        am_hal_gpio_state_write(
            led_context[ui32Handle].config.ui32Pin,
            led_context[ui32Handle].config.ui32ActiveLow ? AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);

        // set ui32OnTime back to 1 as the CTIMER period value cannot be 0 or period
        // actualy value has no visual impact as the CTIMER output is disconnected
        // from the GPIO.
        duty_cycle = 1;
    }
    else if (ui32OnTime == led_context[ui32Handle].sequence->ui32Period)
    {
        // full on
        am_hal_gpio_pinconfig(led_context[ui32Handle].config.ui32Pin, g_AM_HAL_GPIO_OUTPUT_8);
        am_hal_gpio_state_write(
            led_context[ui32Handle].config.ui32Pin,
            led_context[ui32Handle].config.ui32ActiveLow ? AM_HAL_GPIO_OUTPUT_CLEAR : AM_HAL_GPIO_OUTPUT_SET);

        // set ui32OnTime back to period - 1 as the CTIMER period value cannot be 0 or period
        // actualy value has no visual impact as the CTIMER output is disconnected
        // from the GPIO.
        duty_cycle = led_context[ui32Handle].sequence->ui32Period - 1;
    }
    else
    {
        // PWM
        am_hal_ctimer_output_config(
            led_context[ui32Handle].config.ui32Number,
            led_context[ui32Handle].config.ui32Segment,
            led_context[ui32Handle].config.ui32Pin,
            AM_HAL_CTIMER_OUTPUT_NORMAL,
            AM_HAL_GPIO_PIN_DRIVESTRENGTH_8MA);
    }

    if (!(led_context[ui32Handle].index & 1))
    {
        am_hal_ctimer_period_set(
            led_context[ui32Handle].config.ui32Number,
            led_context[ui32Handle].config.ui32Segment,
            led_context[ui32Handle].sequence->ui32Period, duty_cycle);
    }
    else
    {
        am_hal_ctimer_aux_period_set(
            led_context[ui32Handle].config.ui32Number,
            led_context[ui32Handle].config.ui32Segment,
            led_context[ui32Handle].sequence->ui32Period, duty_cycle);
    }
}

void led_interrupt_service(uint32_t ui32Handle)
{
    if (led_context[ui32Handle].sequence == NULL)
    {
        return;
    }

    // if the number of repeat has been executed, stop
    // the timer and set the output to constant low.
    if ((led_context[ui32Handle].count == 0) && (led_context[ui32Handle].repeat > 0))
    {
        am_hal_ctimer_stop(led_context[ui32Handle].config.ui32Number, led_context[ui32Handle].config.ui32Segment);
        am_hal_gpio_pinconfig(led_context[ui32Handle].config.ui32Pin, g_AM_HAL_GPIO_OUTPUT_8);
        am_hal_gpio_state_write(led_context[ui32Handle].config.ui32Pin, led_context[ui32Handle].config.ui32ActiveLow ? AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);
        return;
    }

    led_config_duty_cycle(ui32Handle, led_context[ui32Handle].sequence->pui8Sequence[led_context[ui32Handle].index]);

    // Utilizing a full modulus instead of the faster bit level modulus since it is restricted to
    // a power of two.
    // led_context[ui32Handle].index = (led_context[ui32Handle].index + 1) & (led_context[ui32Handle].sequence->ui32Period - 1);
    led_context[ui32Handle].index = (led_context[ui32Handle].index + 1) % (led_context[ui32Handle].sequence->ui32Period);

    if (led_context[ui32Handle].count > 0)
    {
        led_context[ui32Handle].count--;
    }
}

static void handle_led_off(uint32_t ui32Handle)
{
    if (ui32Handle >= led_num_count)
    {
        return;
    }

    led_context[ui32Handle].status = LED_STATUS_OFF;
    led_context[ui32Handle].sequence = 0;
    am_hal_ctimer_stop(
        led_context[ui32Handle].config.ui32Number,
        led_context[ui32Handle].config.ui32Segment);
    am_hal_gpio_pinconfig(
        led_context[ui32Handle].config.ui32Pin,
        g_AM_HAL_GPIO_OUTPUT_8);
    am_hal_gpio_state_write(
        led_context[ui32Handle].config.ui32Pin,
        led_context[ui32Handle].config.ui32ActiveLow ? AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);
}

static void handle_led_on(uint32_t ui32Handle)
{
    if (ui32Handle >= led_num_count)
    {
        return;
    }

    led_context[ui32Handle].status = LED_STATUS_ACTIVE;
    led_context[ui32Handle].sequence = 0;
    am_hal_ctimer_stop(
        led_context[ui32Handle].config.ui32Number,
        led_context[ui32Handle].config.ui32Segment);
    am_hal_gpio_pinconfig(
        led_context[ui32Handle].config.ui32Pin,
        g_AM_HAL_GPIO_OUTPUT_8);
    am_hal_gpio_state_write(
        led_context[ui32Handle].config.ui32Pin,
        led_context[ui32Handle].config.ui32ActiveLow ? AM_HAL_GPIO_OUTPUT_CLEAR : AM_HAL_GPIO_OUTPUT_SET);
}

static void handle_led_idle(uint32_t ui32Handle)
{
    if (ui32Handle >= led_num_count)
    {
        return;
    }

    led_context[ui32Handle].status = LED_STATUS_IDLE;
    led_context[ui32Handle].sequence = 0;
    am_hal_ctimer_stop(
        led_context[ui32Handle].config.ui32Number,
        led_context[ui32Handle].config.ui32Segment);
    am_hal_gpio_pinconfig(
        led_context[ui32Handle].config.ui32Pin,
        g_AM_HAL_GPIO_OUTPUT_8);
    am_hal_gpio_state_write(
        led_context[ui32Handle].config.ui32Pin,
        led_context[ui32Handle].config.ui32ActiveLow ? AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);
}

static void handle_led_effect(uint32_t ui32Handle, uint32_t ui32EffectId, uint32_t ui32EffectRepeat)
{
    led_context[ui32Handle].effect = led_effect_id[ui32EffectId];
    if (!(led_context[ui32Handle].effect < LED_EFFECT_MAX))
    {
        return;
    }

    if ((led_context[ui32Handle].sequence == led_sequence_table[led_context[ui32Handle].effect])
     && (led_context[ui32Handle].count > 0))
    {
        return;
    }

    led_context[ui32Handle].sequence = led_sequence_table[led_context[ui32Handle].effect];

    if (led_context[ui32Handle].sequence)
    {
        led_context[ui32Handle].status = LED_STATUS_ACTIVE;
        led_context[ui32Handle].index = 0;
        led_context[ui32Handle].repeat = ui32EffectRepeat;
        led_context[ui32Handle].count =
            led_context[ui32Handle].repeat * led_context[ui32Handle].sequence->ui32Period; 

        am_hal_ctimer_config_single(
            led_context[ui32Handle].config.ui32Number,
            led_context[ui32Handle].config.ui32Segment,
            (AM_HAL_CTIMER_FN_PWM_REPEAT |
             AM_HAL_CTIMER_INT_ENABLE |
             led_context[ui32Handle].sequence->ui32Clock));

        am_hal_ctimer_period_set(
            led_context[ui32Handle].config.ui32Number,
            led_context[ui32Handle].config.ui32Segment,
            led_context[ui32Handle].sequence->ui32Period, 1);

        am_hal_ctimer_aux_period_set(
            led_context[ui32Handle].config.ui32Number,
            led_context[ui32Handle].config.ui32Segment,
            led_context[ui32Handle].sequence->ui32Period, 1);

        am_hal_ctimer_int_enable(led_context[ui32Handle].config.ui32Interrupt);
        am_hal_ctimer_start(
            led_context[ui32Handle].config.ui32Number,
            led_context[ui32Handle].config.ui32Segment);
    }
}

static void led_task_setup(void)
{
    memset(led_effect_id, 0, sizeof(led_effect_id));
    led_register_effect(&led_effect_id[LED_EFFECT_BREATHING], &led_effect_breathing);
    led_register_effect(&led_effect_id[LED_EFFECT_PULSE1], &led_effect_pulse1);
    led_register_effect(&led_effect_id[LED_EFFECT_PULSE2], &led_effect_pulse2);
    led_register_effect(&led_effect_id[LED_EFFECT_PULSE3], &led_effect_pulse3);
    led_register_effect(&led_effect_id[LED_EFFECT_SOS], &led_effect_sos);
}

static void led_task_loop(void)
{
    led_command_t command;
    uint32_t ui32Id;

    if (xQueueReceive(led_task_queue, &command, portMAX_DELAY) == pdFAIL)
    {
        return;
    }

    if (command.ui32Handle >= led_num_count)
    {
        return;
    }

    switch(command.ui32Id)
    {
    case LED_EFFECT_OFF:
        handle_led_off(command.ui32Handle);
        break;

    case LED_EFFECT_ON:
        handle_led_on(command.ui32Handle);
        break;

    case LED_EFFECT_IDLE:
        handle_led_idle(command.ui32Handle);
        break;

    default:
        handle_led_effect(command.ui32Handle, command.ui32Id, command.ui32Repeat);
        break;
    }
}

static void led_task(void *parameter)
{
#if defined(CLI_ENABLE)
    led_task_cli_register();
#endif

    led_task_setup();
    while (1)
    {
        led_task_loop();
    }
}

void led_task_create(uint32_t priority)
{
    led_num_count = 0;
    memset(led_context, 0, sizeof(led_context_t) * LED_NUM_MAX);

    led_task_queue = xQueueCreate(10, sizeof(led_command_t));
    xTaskCreate(led_task, "led", 512, 0, priority, &led_task_handle);
}

void led_config(uint32_t *pui32Handle, const led_config_t *psConfig)
{
    if (led_num_count >= LED_NUM_MAX)
    {
        return;
    }

    if (pui32Handle == NULL)
    {
        return;
    }

    if (psConfig == NULL)
    {
        return;
    }

    *pui32Handle = led_num_count++;
    led_context[*pui32Handle].ui32Handle = *pui32Handle;
    led_context[*pui32Handle].effect = LED_EFFECT_IDLE;
    led_context[*pui32Handle].status = LED_STATUS_IDLE;
    led_context[*pui32Handle].sequence = NULL;
    led_context[*pui32Handle].repeat = 0;
    led_context[*pui32Handle].count = 0;
    led_context[*pui32Handle].index = 0;
    memcpy(&(led_context[*pui32Handle].config), psConfig, sizeof(led_config_t));
    am_hal_ctimer_int_register(led_context[*pui32Handle].config.ui32Interrupt, led_context[*pui32Handle].config.pfnInterruptService);
    am_hal_ctimer_int_disable(led_context[*pui32Handle].config.ui32Interrupt);
    NVIC_EnableIRQ(CTIMER_IRQn);
}

void led_config_list(uint32_t *pui32HandleList, led_config_t **psConfigList, uint32_t *pui32Length)
{
    uint32_t ui32Index;

    if (pui32HandleList == NULL)
    {
        return;
    }

    if (pui32Length == NULL)
    {
        return;
    }

    for (ui32Index = 0; ui32Index < led_num_count; ui32Index++)
    {
        pui32HandleList[ui32Index] = led_context[ui32Index].ui32Handle;

        if (psConfigList)
        {
            psConfigList[ui32Index] = &(led_context[ui32Index].config);
        }
    }

    *pui32Length = led_num_count;
}

void led_register_effect(uint32_t *pui32Id, const led_sequence_t *psEffect)
{
    if (led_effect_count >= LED_EFFECT_MAX)
    {
        return;
    }

    if (pui32Id)
    {
        *pui32Id = led_effect_count;
    }
    led_effect_id[led_effect_count] = led_effect_count;
    led_sequence_table[led_effect_count] = psEffect;
    led_effect_count++;
}

void led_send(led_command_t *psCommand)
{
    BaseType_t xHigherPriorityTaskWoken;

    if (led_task_queue)
    {
        if (xPortIsInsideInterrupt() == pdTRUE)
        {
            xHigherPriorityTaskWoken = pdFALSE;
            xQueueSendFromISR(led_task_queue, psCommand, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
        else
        {
            xQueueSend(led_task_queue, psCommand, portMAX_DELAY);
        }
    }
}

led_status_t led_status_get(uint32_t ui32Handle)
{
    if (led_num_count >= LED_NUM_MAX)
    {
        return LED_STATUS_UNCONFIGURED;
    }

    return led_context[ui32Handle].status;
}
