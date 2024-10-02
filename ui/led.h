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

#define LED_COMMAND_OFF         (1000)
#define LED_COMMAND_ON          (1001)
#define LED_COMMAND_IDLE        (1002)

/**
 * @brief Predefined LED effects.
 * 
 */
enum led_command_e {
    LED_COMMAND_BREATHING = 0,
    LED_COMMAND_PULSE1,
    LED_COMMAND_PULSE2,
    LED_COMMAND_PULSE3,
    LED_COMMAND_SOS,
    LED_COMMAND_MAX,
};

typedef enum {
    LED_STATUS_OFF = 0,
    LED_STATUS_IDLE,
    LED_STATUS_ACTIVE
} led_status_t;

#define LED_EFFECT_MAX     (LED_COMMAND_MAX + 8)

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
 */
typedef struct {
    uint32_t ui32Number;
    uint32_t ui32Segment;
    uint32_t ui32Pin;
    uint32_t ui32Interrupt;
    uint32_t ui32ActiveLow;
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
} led_effect_t;

/**
 * @brief LED effect to execute.
 * 
 * @param ui32Id The effect ID value.
 * 
 * @param ui32Repeat Number of times to repeat the effect.  Zero for indefinite.
 */
typedef struct {
    uint32_t ui32Id;
    uint32_t ui32Repeat;
} led_command_t;

/**
 * @brief Configure LED.
 * 
 * @param psConfig LED configuration.
 */
extern void led_config(const led_config_t *psConfig);

/**
 * @brief Register an LED effect and return an identifier.
 * 
 * @param pui32Id Identifier of the LED effect for use in led_send.
 * 
 * @param psEffect Pointer to the LED effect definition.  Cannot be defined on the stack
 *   as there is no deep-copy performed.
 */
extern void led_register_effect(uint32_t *pui32Id, const led_effect_t *psEffect);

/**
 * @brief Request to execute an LED effect.
 * 
 * @param psCommand Pointer to the LED command.
 */
extern void led_send(led_command_t *psCommand);

/**
 * @brief Get LED task status.
 * 
 * @return 1 if LED effect is active.  0 if idle.
 */
extern led_status_t led_status_get();

#ifdef __cplusplus
}
#endif

#endif