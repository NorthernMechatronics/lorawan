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

uint32_t led_effect_id[LED_EFFECT_MAX];

static TaskHandle_t led_task_handle;
static QueueHandle_t led_task_queue;

static led_status_t led_status;
static led_config_t led_cfg;
static const led_effect_t* led_effect_table[LED_EFFECT_MAX];
static const led_effect_t* led_effect;
static uint32_t led_effect_max_index;
static uint32_t led_effect_repeat;
static volatile uint32_t led_effect_sequence_index;
static volatile uint32_t led_effect_count;

static void led_config_duty_cycle(uint32_t on_time)
{
    uint32_t duty_cycle = on_time;

    if (on_time == 0)
    {
        // full off
        am_hal_gpio_pinconfig(led_cfg.ui32Pin, g_AM_HAL_GPIO_OUTPUT_8);
        am_hal_gpio_state_write(led_cfg.ui32Pin, led_cfg.ui32ActiveLow ? AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);

        // set on_time back to 1 as the CTIMER period value cannot be 0 or period
        // actualy value has no visual impact as the CTIMER output is disconnected
        // from the GPIO.
        duty_cycle = 1;
    }
    else if (on_time == led_effect->ui32Period)
    {
        // full on
        am_hal_gpio_pinconfig(led_cfg.ui32Pin, g_AM_HAL_GPIO_OUTPUT_8);
        am_hal_gpio_state_write(led_cfg.ui32Pin, led_cfg.ui32ActiveLow ? AM_HAL_GPIO_OUTPUT_CLEAR : AM_HAL_GPIO_OUTPUT_SET);

        // set on_time back to period - 1 as the CTIMER period value cannot be 0 or period
        // actualy value has no visual impact as the CTIMER output is disconnected
        // from the GPIO.
        duty_cycle = led_effect->ui32Period - 1;
    }
    else
    {
        // PWM
        am_hal_ctimer_output_config(led_cfg.ui32Number, led_cfg.ui32Segment, led_cfg.ui32Pin,
            AM_HAL_CTIMER_OUTPUT_NORMAL,
            AM_HAL_GPIO_PIN_DRIVESTRENGTH_8MA);
    }

    if (!(led_effect_sequence_index & 1))
    {
        am_hal_ctimer_period_set(led_cfg.ui32Number, led_cfg.ui32Segment, led_effect->ui32Period, duty_cycle);
    }
    else
    {
        am_hal_ctimer_aux_period_set(led_cfg.ui32Number, led_cfg.ui32Segment, led_effect->ui32Period, duty_cycle);
    }
}

static void led_ctimer_isr(void)
{
    if (led_effect == NULL)
    {
        return;
    }

    // if the number of repeat has been executed, stop
    // the timer and set the output to constant low.
    if ((led_effect_count == 0) && (led_effect_repeat > 0))
    {
        am_hal_ctimer_stop(led_cfg.ui32Number, led_cfg.ui32Segment);
        am_hal_gpio_pinconfig(led_cfg.ui32Pin, g_AM_HAL_GPIO_OUTPUT_8);
        am_hal_gpio_state_write(led_cfg.ui32Pin, led_cfg.ui32ActiveLow ? AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);
        return;
    }

    led_config_duty_cycle(led_effect->pui8Sequence[led_effect_sequence_index]);

    // Utilizing a full modulus instead of the faster bit level modulus since it is restricted to
    // a power of two.
    // led_effect_sequence_index = (led_effect_sequence_index + 1) & (led_effect->ui32Period - 1);
    led_effect_sequence_index = (led_effect_sequence_index + 1) % (led_effect->ui32Period);

    if (led_effect_count > 0)
    {
        led_effect_count--;
    }
}

static void led_task_setup(void)
{
    memset(led_effect_id, 0, sizeof(led_effect_id));
    led_register_effect(&led_effect_id[LED_COMMAND_BREATHING], &led_effect_breathing);
    led_register_effect(&led_effect_id[LED_COMMAND_PULSE1], &led_effect_pulse1);
    led_register_effect(&led_effect_id[LED_COMMAND_PULSE2], &led_effect_pulse2);
    led_register_effect(&led_effect_id[LED_COMMAND_PULSE3], &led_effect_pulse3);
    led_register_effect(&led_effect_id[LED_COMMAND_SOS], &led_effect_sos);
}

static void led_task_loop(void)
{
    led_command_t command;
    uint32_t ui32Id;

    if (xQueueReceive(led_task_queue, &command, portMAX_DELAY) == pdFAIL)
    {
        return;
    }

    switch(command.ui32Id)
    {
    case LED_COMMAND_OFF:
        led_status = LED_STATUS_OFF;
        led_effect = 0;
        am_hal_ctimer_stop(led_cfg.ui32Number, led_cfg.ui32Segment);
        am_hal_gpio_pinconfig(led_cfg.ui32Pin, g_AM_HAL_GPIO_OUTPUT_8);
        am_hal_gpio_state_write(led_cfg.ui32Pin, led_cfg.ui32ActiveLow ? AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);
        break;

    case LED_COMMAND_ON:
        led_status = LED_STATUS_ACTIVE;
        led_effect = 0;
        am_hal_ctimer_stop(led_cfg.ui32Number, led_cfg.ui32Segment);
        am_hal_gpio_pinconfig(led_cfg.ui32Pin, g_AM_HAL_GPIO_OUTPUT_8);
        am_hal_gpio_state_write(led_cfg.ui32Pin, led_cfg.ui32ActiveLow ? AM_HAL_GPIO_OUTPUT_CLEAR : AM_HAL_GPIO_OUTPUT_SET);
        break;

    case LED_COMMAND_IDLE:
        led_status = LED_STATUS_IDLE;
        led_effect = 0;
        am_hal_ctimer_stop(led_cfg.ui32Number, led_cfg.ui32Segment);
        am_hal_gpio_pinconfig(led_cfg.ui32Pin, g_AM_HAL_GPIO_OUTPUT_8);
        am_hal_gpio_state_write(led_cfg.ui32Pin, led_cfg.ui32ActiveLow ? AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);
        break;

    default:
        ui32Id = led_effect_id[command.ui32Id];
        if (!(ui32Id < LED_EFFECT_MAX))
        {
            return;
        }

        if ((led_effect == led_effect_table[ui32Id]) && (led_effect_count > 0))
        {
            return;
        }

        led_effect = led_effect_table[ui32Id];
        if (led_effect)
        {
            led_status = LED_STATUS_ACTIVE;
            led_effect_sequence_index = 0;
            led_effect_repeat = command.ui32Repeat;
            led_effect_count = led_effect_repeat * led_effect->ui32Period;

            am_hal_ctimer_config_single(led_cfg.ui32Number, led_cfg.ui32Segment,
                (AM_HAL_CTIMER_FN_PWM_REPEAT | AM_HAL_CTIMER_INT_ENABLE | led_effect->ui32Clock));

            am_hal_ctimer_period_set(led_cfg.ui32Number, led_cfg.ui32Segment, led_effect->ui32Period, 1);
            am_hal_ctimer_aux_period_set(led_cfg.ui32Number, led_cfg.ui32Segment, led_effect->ui32Period, 1);

            am_hal_ctimer_int_enable(led_cfg.ui32Interrupt);
            am_hal_ctimer_start(led_cfg.ui32Number, led_cfg.ui32Segment);
        }
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
    memset(&led_cfg, 0, sizeof(led_config_t));
    led_effect = NULL;
    led_effect_sequence_index = 0;
    led_effect_max_index = 0;
    led_status = LED_STATUS_IDLE;

    led_task_queue = xQueueCreate(10, sizeof(led_command_t));
    xTaskCreate(led_task, "led", 512, 0, priority, &led_task_handle);
}

void led_config(const led_config_t *psConfig)
{
    memcpy(&led_cfg, psConfig, sizeof(led_config_t));

    am_hal_ctimer_int_register(led_cfg.ui32Interrupt, led_ctimer_isr);
    am_hal_ctimer_int_disable(led_cfg.ui32Interrupt);
    NVIC_EnableIRQ(CTIMER_IRQn);
}

void led_register_effect(uint32_t *pui32Id, const led_effect_t *psEffect)
{
    if (led_effect_max_index >= LED_EFFECT_MAX)
    {
        return;
    }

    if (pui32Id)
    {
        *pui32Id = led_effect_max_index;
    }
    led_effect_id[led_effect_max_index] = led_effect_max_index;
    led_effect_table[led_effect_max_index] = psEffect;
    led_effect_max_index++;
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

led_status_t led_status_get(void)
{
    return led_status;
}
