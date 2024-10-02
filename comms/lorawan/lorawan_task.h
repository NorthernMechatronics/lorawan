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
#ifndef _LORAWAN_TASK_H_
#define _LORAWAN_TASK_H_

#include <LmHandler.h>
#include <LmhpFragmentation.h>
#include "lorawan.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LORAWAN_COMMAND_QUEUE_MAX_SIZE  (8)
#define LORAWAN_TRANSMIT_QUEUE_MAX_SIZE (8)

typedef enum
{
    LORAWAN_START,
    LORAWAN_STOP,
    LORAWAN_JOIN,
    LORAWAN_SYNC_APP,
    LORAWAN_SYNC_MAC,
    LORAWAN_CLASS_SET,
} lorawan_command_e;

typedef struct
{
    lorawan_command_e eCommand;
    void *pvParameters;
} lorawan_command_t;

extern lorawan_event_callback_t lorawan_event_callback_list[LORAWAN_EVENTS];
extern uint32_t lorawan_tracing_enabled;

extern void lorawan_task_create(uint32_t ui32Priority);
extern void lorawan_task_wake();

extern void lmh_callbacks_setup(LmHandlerCallbacks_t *cb);
extern void lmhp_fragmentation_setup(LmhpFragmentationParams_t *parameters);

extern void lorawan_send_command(lorawan_command_t *psCommand);

#ifdef __cplusplus
}
#endif

#endif
