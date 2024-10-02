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
#ifndef _BUTTON_H_
#define _BUTTON_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Button press sequence callback function.
 * 
 */
typedef void (*sequence_callback_t)(void);

/**
 * @brief Configure the GPIO for button press sequence handling.
 * 
 * @param pui32Handle Button press handle.
 * 
 * @param ui32Pin GPIO pin number.
 * 
 * @param sConfig GPIO configuration.
 *
 * @param ui32ActiveLow Button is nominally high and a press is active low.
 */
extern void button_config(uint32_t *pui32Handle, uint32_t ui32Pin, am_hal_gpio_pincfg_t sConfig, uint32_t ui32ActiveLow);

/**
 * @brief Register a callback function to a button press sequence.
 *   For example a sequence of four button presses consisting of short, short, long, long is:
 *   button_sequence_register(handle, 4, 0b1100, cb);
 * 
 * @param ui32Handle Button press handle.
 * 
 * @param ui32Size Number of button presses in a squence up to 24 presses.
 * 
 * @param ui32Value A sequence of long (1) and short (0) presses starting from LSB to MSB and up to 24-bit.
 * 
 * @param pfnCallback Callback function to execute when the sequence is pressed.
 */
extern void button_sequence_register(uint32_t ui32Handle, uint32_t ui32Size, uint32_t ui32Value, sequence_callback_t pfnCallback);

/**
 * @brief Unregister a callback function to a button press sequence.
 *   For example a sequence of four button presses consisting of short, short, long, long is:
 *   button_sequence_register(handle, 4, 0b1100, cb);
 * 
 * @param ui32Handle Button press handle.
 * 
 * @param ui32Size Number of button presses in a squence up to 24 presses.
 * 
 * @param ui32Value A sequence of long (1) and short (0) presses starting from LSB to MSB and up to 24-bit.
 * 
 * @param pfnCallback Callback function to execute when the sequence is pressed.
 */
extern void button_sequence_unregister(uint32_t ui32Handle, uint32_t ui32Size, uint32_t ui32Value, sequence_callback_t pfnCallback);

#ifdef __cplusplus
}
#endif

#endif