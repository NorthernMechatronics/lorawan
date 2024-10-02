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

#include "application_task_cli.h"

#define COMMAND_LINE_BUFFER_MAX     (128)

static portBASE_TYPE application_task_cli_entry(char *pui8OutBuffer,
                                                size_t ui32OutBufferLength,
                                                const char *pui8Command);

static CLI_Command_Definition_t application_task_cli_definition = {
    (const char *const) "app",
    (const char *const) "app    :  application control\r\n",
    application_task_cli_entry,
    -1};

static size_t argc;
static char *argv[8];
static char argz[COMMAND_LINE_BUFFER_MAX];

void application_task_cli_register()
{
    FreeRTOS_CLIRegisterCommand(&application_task_cli_definition);
    argc = 0;
}

static void help(char *pui8OutBuffer, size_t argc, char **argv)
{
    am_util_stdio_printf("\r\nusage: app <command>\r\n");
    am_util_stdio_printf("\r\n");
    am_util_stdio_printf("supported commands are:\r\n");
    am_util_stdio_printf("  help     display help message\r\n");
    am_util_stdio_printf("  reset    perform a soft reset\r\n");
    am_util_stdio_printf("  version  output version information\r\n\r\n");
}

static void version(char *pui8OutBuffer, size_t argc, char **argv)
{
    am_util_stdio_printf("\r\nSDK version %s\r\n", nm_sdk_version);
    am_util_stdio_printf("Application template version %s\r\n", nm_app_version);
}

portBASE_TYPE
application_task_cli_entry(char *pui8OutBuffer, size_t ui32OutBufferLength, const char *pui8Command)
{
    pui8OutBuffer[0] = 0;

    memset(argz, 0, COMMAND_LINE_BUFFER_MAX);
    strcpy(argz, pui8Command);
    FreeRTOS_CLIExtractParameters(argz, &argc, argv);

    if (strcmp(argv[1], "help") == 0)
    {
        help(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "reset") == 0)
    {
        NVIC_SystemReset();
    }
    else if (strcmp(argv[1], "version") == 0)
    {
        version(pui8OutBuffer, argc, argv);
    }
    else
    {
        help(pui8OutBuffer, argc, argv);
    }

    return pdFALSE;
}
