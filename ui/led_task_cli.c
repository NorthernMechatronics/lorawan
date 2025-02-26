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
#include <stdlib.h>
#include <string.h>

#include <am_mcu_apollo.h>
#include <am_util.h>

#include <FreeRTOS.h>
#include <FreeRTOS_CLI.h>

#include "nm_app_version.h"
#include "nm_sdk_version.h"

#include "led.h"
#include "led_task_cli.h"

extern uint32_t led_effect_id[LED_EFFECT_MAX];

#define COMMAND_LINE_BUFFER_MAX     (128)

static portBASE_TYPE led_task_cli_entry(char *pui8OutBuffer,
                                                size_t ui32OutBufferLength,
                                                const char *pui8Command);

static CLI_Command_Definition_t led_task_cli_definition = {
    (const char *const) "led",
    (const char *const) "led    :  led control\r\n",
    led_task_cli_entry,
    -1};

static size_t argc;
static char *argv[8];
static char argz[COMMAND_LINE_BUFFER_MAX];

void led_task_cli_register()
{
    FreeRTOS_CLIRegisterCommand(&led_task_cli_definition);
    argc = 0;
}

static void help(char *pui8OutBuffer, size_t argc, char **argv)
{
    am_util_stdio_printf("\r\nusage: led <command>\r\n");
    am_util_stdio_printf("\r\n");
    am_util_stdio_printf("supported commands are:\r\n");
    am_util_stdio_printf("  list    display configured LED and handles\r\n");
    am_util_stdio_printf("  off     <handle> turn off LED\r\n");
    am_util_stdio_printf("  on      <handle> turn on LED\r\n");
    am_util_stdio_printf("  idle    <handle> application layer controlled\r\n");
    am_util_stdio_printf("  effect  <handle> <value> [count]\r\n");
    am_util_stdio_printf("          effect values are:\r\n");
    am_util_stdio_printf("            breathing\r\n");
    am_util_stdio_printf("            pulse1\r\n");
    am_util_stdio_printf("            pulse2\r\n");
    am_util_stdio_printf("            pulse3\r\n");
    am_util_stdio_printf("            sos\r\n");
    am_util_stdio_printf("          Repeat LED effect for count;\r\n");
    am_util_stdio_printf("          indefinitely if count is zero or omitted.\r\n");
    am_util_stdio_printf("\r\n");
}

static void list(char *pui8OutBuffer, size_t argc, char **argv)
{
    uint32_t handle_list[LED_NUM_MAX];
    led_config_t *config_list[LED_NUM_MAX];
    uint32_t length;

    led_config_list(handle_list, config_list, &length);

    if (length == 0)
    {
        am_util_stdio_printf("No LED configured\r\n");
        return;
    }

    am_util_stdio_printf("\r\n");
    am_util_stdio_printf("LED Configuration\r\n");
    for (uint32_t i = 0; i < length; i++)
    {
        am_util_stdio_printf("Handle: %3d, Pin: %3d\r\n", handle_list[i], config_list[i]->ui32Pin);
    }
    am_util_stdio_printf("\r\n");
}

static void off(char *pui8OutBuffer, size_t argc, char **argv)
{
    char *end_ptr;
    uint32_t handle_id;
    led_command_t command;

    if (argc < 3)
    {
        am_util_stdio_printf("\r\nMissing LED handle id.\r\n");
        help(pui8OutBuffer, argc, argv);
        return;
    }

    handle_id = strtol(argv[2], &end_ptr, 10);
    command.ui32Handle = handle_id;
    command.ui32Id = LED_EFFECT_OFF;
    command.ui32Repeat = 0;
    led_send(&command);
}

static void on(char *pui8OutBuffer, size_t argc, char **argv)
{
    char *end_ptr;
    uint32_t handle_id;
    led_command_t command;

    if (argc < 3)
    {
        am_util_stdio_printf("\r\nMissing LED handle id.\r\n");
        help(pui8OutBuffer, argc, argv);
        return;
    }

    handle_id = strtol(argv[2], &end_ptr, 10);
    command.ui32Handle = handle_id;
    command.ui32Id = LED_EFFECT_ON;
    command.ui32Repeat = 0;
    led_send(&command);
}

static void idle(char *pui8OutBuffer, size_t argc, char **argv)
{
    char *end_ptr;
    uint32_t handle_id;
    led_command_t command;

    if (argc < 3)
    {
        am_util_stdio_printf("\r\nMissing LED handle id.\r\n");
        help(pui8OutBuffer, argc, argv);
        return;
    }

    handle_id = strtol(argv[2], &end_ptr, 10);
    command.ui32Handle = handle_id;
    command.ui32Id = LED_EFFECT_IDLE;
    command.ui32Repeat = 0;
    led_send(&command);
}

static void effect(char *pui8OutBuffer, size_t argc, char **argv)
{
    char *end_ptr;
    uint32_t handle_id;
    led_command_t command;

    if (argc < 3)
    {
        am_util_stdio_printf("\r\nMissing LED handle id.\r\n");
        help(pui8OutBuffer, argc, argv);
        return;
    }

    handle_id = strtol(argv[2], &end_ptr, 10);
    command.ui32Handle = handle_id;

    uint32_t effect, repeat;
    if (argc < 4)
    {
        am_util_stdio_printf("\r\nMissing effect value.\r\n");
        help(pui8OutBuffer, argc, argv);
        return;
    }

    if (strcmp(argv[3], "breathing") == 0)
    {
        effect = led_effect_id[LED_EFFECT_BREATHING];
    }
    else if (strcmp(argv[3], "pulse1") == 0)
    {
        effect = led_effect_id[LED_EFFECT_PULSE1];
    }
    else if (strcmp(argv[3], "pulse2") == 0)
    {
        effect = led_effect_id[LED_EFFECT_PULSE2];
    }
    else if (strcmp(argv[3], "pulse3") == 0)
    {
        effect = led_effect_id[LED_EFFECT_PULSE3];
    }
    else if (strcmp(argv[3], "sos") == 0)
    {
        effect = led_effect_id[LED_EFFECT_SOS];
    }
    else
    {
        am_util_stdio_printf("\r\nInvalid effect specified.\r\n");
        help(pui8OutBuffer, argc, argv);
        return;
    }

    repeat = 0;
    if (argc == 5)
    {
        repeat = strtol(argv[4], NULL, 10);
    }

    command.ui32Handle = handle_id;
    command.ui32Id = effect;
    command.ui32Repeat = repeat;
    led_send(&command);
}

portBASE_TYPE
led_task_cli_entry(char *pui8OutBuffer, size_t ui32OutBufferLength, const char *pui8Command)
{

    pui8OutBuffer[0] = 0;

    memset(argz, 0, COMMAND_LINE_BUFFER_MAX);
    strcpy(argz, pui8Command);
    FreeRTOS_CLIExtractParameters(argz, &argc, argv);

    if (strcmp(argv[1], "help") == 0)
    {
        help(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "list") == 0)
    {
        list(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "off") == 0)
    {
        off(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "on") == 0)
    {
        on(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "idle") == 0)
    {
        idle(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "effect") == 0)
    {
        effect(pui8OutBuffer, argc, argv);
    }
    else
    {
        help(pui8OutBuffer, argc, argv);
    }

    return pdFALSE;
}
