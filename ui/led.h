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
#ifndef _LED_COMMAND_H_
#define _LED_COMMAND_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LED_NUM_MAX            (3)

#define LED_EFFECT_OFF         (1000)
#define LED_EFFECT_ON          (1001)
#define LED_EFFECT_IDLE        (1002)

/**
 * @brief Predefined LED effects.
 * 
 */
typedef enum led_effect_e {
    LED_EFFECT_BREATHING = 0,
    LED_EFFECT_PULSE1,
    LED_EFFECT_PULSE2,
    LED_EFFECT_PULSE3,
    LED_EFFECT_SOS,
    LED_EFFECT_BUILTIN_MAX,
} led_effect_t;

typedef enum {
    LED_STATUS_UNCONFIGURED = 0,
    LED_STATUS_OFF,
    LED_STATUS_IDLE,
    LED_STATUS_ACTIVE,
} led_status_t;

#define LED_EFFECT_MAX     (LED_EFFECT_BUILTIN_MAX + 8)

typedef void (*led_ctimer_isr_t)(void);

/**
 * @brief LED timer and GPIO parameters.
 * 
 * @param ui32Number CTIMER number.
 * 
 * @param ui32Segment CTIMER segment.
 * 
 * @param ui32Pin GPIO pin connected to the CTIMER output.
 * 
 * @param ui32Interrupt CTIMER interupt mask.
 * 
 * @param ui32ActiveLow LED is active low.
 * 
 * @param pfnInterruptService CTIMER interrupt handler.
 */
typedef struct {
    uint32_t ui32Number;
    uint32_t ui32Segment;
    uint32_t ui32Pin;
    uint32_t ui32Interrupt;
    uint32_t ui32ActiveLow;
    led_ctimer_isr_t pfnInterruptService;
} led_config_t;

/**
 * @brief LED effect definition.
 * 
 * @param ui32Clock CTIMER Clock source.  See am_hal_ctimer.h for details.
 * 
 * @param ui32Period CTIMER Clock period.
 * 
 * @param pui8Sequence A byte sequence indicating the LED intensity with 0 being the lowest
 *   and a value equal to period being the maximum.
 */
typedef struct {
    uint32_t  ui32Clock;
    uint32_t  ui32Period;
    const uint8_t pui8Sequence[];
} led_sequence_t;

/**
 * @brief LED effect to execute.
 * 
 * @param ui32Handle LED handle.
 * 
 * @param ui32Id The effect ID value.
 * 
 * @param ui32Repeat Number of times to repeat the effect.  Zero for indefinite.
 */
typedef struct {
    uint32_t ui32Handle;
    uint32_t ui32Id;
    uint32_t ui32Repeat;
} led_command_t;

/**
 * @brief Configure LED.
 * 
 * @param ui32Handle pointer to the LED handle.
 * 
 * @param psConfig LED configuration.
 */
extern void led_config(uint32_t *pui32Handle, const led_config_t *psConfig);

/**
 * @brief Return a list of configured LED.
 * 
 * @param pui32HandleList pointer to the handle list of configured LEDs.
 *
 * @param psConfigList optional pointer to the configuration list of configured LEDs.  Set to NULL if not used.
 *
 * @param pui32Count length of the list.
 */
extern void led_config_list(uint32_t *pui32HandleList, led_config_t **psConfigList, uint32_t *pui32Length);

/**
 * @brief Register an LED effect and return an identifier.
 * 
 * @param pui32Id Identifier of the LED effect for use in led_send.
 * 
 * @param psEffect Pointer to the LED effect definition.  Cannot be defined on the stack
 *   as there is no deep-copy performed.
 */
extern void led_register_effect(uint32_t *pui32Id, const led_sequence_t *psEffect);

/**
 * @brief Request to execute an LED effect.
 * 
 * @param psCommand Pointer to the LED command.
 */
extern void led_send(led_command_t *psCommand);

/**
 * @brief Get LED task status.
 * 
 * @param ui32Handle LED handle
 * 
 * @return led_status_t
 */
extern led_status_t led_status_get(uint32_t ui32Handle);

/**
 * @brief To be called in the LED CTIMER interrupt
 * 
 * @param ui32Handle LED handle
 */
extern void led_interrupt_service(uint32_t ui32Handle);

#ifdef __cplusplus
}
#endif

#endif