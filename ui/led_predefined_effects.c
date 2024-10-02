/*
 *  BSD 3-Clause License
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
#include <am_mcu_apollo.h>

#include "led_predefined_effects.h"

#define LED_EFFECT_PERIOD_BREATHING     (128)
const led_effect_t led_effect_breathing = {
    .ui32Clock = AM_HAL_CTIMER_XT_DIV4,
    .ui32Period = LED_EFFECT_PERIOD_BREATHING,
    .pui8Sequence = {
         1,   2,   2,   2,   3,   3,   4,   5,
         6,   7,   8,   9,  10,  12,  14,  15,
        18,  20,  22,  25,  28,  31,  35,  38,
        42,  46,  51,  55,  60,  64,  69,  74,
        79,  84,  88,  93,  98, 102, 106, 110,
       114, 117, 120, 122, 124, 125, 126, 127,
       127, 126, 125, 124, 122, 120, 117, 114,
       110, 106, 102,  98,  93,  88,  84,  79,
        74,  69,  64,  60,  55,  51,  46,  42,
        38,  35,  31,  28,  25,  22,  20,  18,
        15,  14,  12,  10,   9,   8,   7,   6,
         5,   4,   3,   3,   2,   2,   2,   1,
         0,   0,   0,   0,   0,   0,   0,   0,
         0,   0,   0,   0,   0,   0,   0,   0,
         0,   0,   0,   0,   0,   0,   0,   0,
         0,   0,   0,   0,   0,   0,   0,   0,
    },
};

#define LED_EFFECT_PERIOD_PULSE1        (16)
const led_effect_t led_effect_pulse1 = {
    .ui32Clock = AM_HAL_CTIMER_XT_256HZ,
    .ui32Period = LED_EFFECT_PERIOD_PULSE1,
    .pui8Sequence = {
        16, 16,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
    },
};

#define LED_EFFECT_PERIOD_PULSE2        (16)
const led_effect_t led_effect_pulse2 = {
    .ui32Clock = AM_HAL_CTIMER_XT_256HZ,
    .ui32Period = LED_EFFECT_PERIOD_PULSE2,
    .pui8Sequence = {
        16, 16,  0, 16, 16,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
    },
};

#define LED_EFFECT_PERIOD_PULSE3        (20)
const led_effect_t led_effect_pulse3 = {
    .ui32Clock = AM_HAL_CTIMER_XT_256HZ,
    .ui32Period = LED_EFFECT_PERIOD_PULSE3,
    .pui8Sequence = {
        20, 20,  0,  0, 20, 20,  0,  0,
        20, 20,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,
    },
};

#define LED_EFFECT_PERIOD_SOS           (64)
const led_effect_t led_effect_sos = {
    .ui32Clock = AM_HAL_CTIMER_LFRC_512HZ,
    .ui32Period = LED_EFFECT_PERIOD_SOS,
    .pui8Sequence = {
        64, 64,  0,  0, 64, 64,  0,  0,
        64, 64,  0,  0, 64, 64, 64, 64,
        64,  0,  0, 64, 64, 64, 64, 64,
         0,  0, 64, 64, 64, 64, 64,  0,
         0, 64, 64,  0,  0, 64, 64,  0,
         0, 64, 64,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,
    },
};
