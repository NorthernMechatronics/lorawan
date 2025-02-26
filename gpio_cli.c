/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2020, Northern Mechatronics, Inc.
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
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <am_mcu_apollo.h>
#include <am_util.h>

#include <FreeRTOS.h>
#include <FreeRTOS_CLI.h>
#include <task.h>

#include "gpio_cli.h"

#define MAX_GPIO_PINS 50

#define COMMAND_LINE_BUFFER_MAX     (128)

static uint8_t gReservedPins[] = {
    20, 21, 41,                    // SWD
    22, 23, 33, 37,                // J-Link VCOM
    16, 18, 19,                    // Push-Buttons
    36, 38, 39, 40, 42, 43, 44, 47 // SX1262
};

static size_t MAX_RESERVED_PINS = sizeof(gReservedPins);

portBASE_TYPE gpio_cli_entry(char *pui8OutBuffer, size_t ui32OutBufferLength,
                             const char *pui8Command);

const CLI_Command_Definition_t gpio_cli_definition = {
    (const char *const) "gpio",
    (const char *const) "gpio   :  NM1801xx GPIO control.\r\n", gpio_cli_entry, -1};

static size_t argc;
static char *argv[8];
static char argz[COMMAND_LINE_BUFFER_MAX];

void gpio_cli_register(void)
{
    FreeRTOS_CLIRegisterCommand(&gpio_cli_definition);
}

static void help(char *pui8OutBuffer, size_t argc, char **argv)
{
    am_util_stdio_printf("usage: gpio <command> [<args>]\r\n");
    am_util_stdio_printf("\r\n");
    am_util_stdio_printf("Supported commands are:\r\n");
    am_util_stdio_printf("  init    initialize <GPIO> as <input|output>\r\n");
    am_util_stdio_printf("  deinit  de-initialize <GPIO>\r\n");
    am_util_stdio_printf("  write   set <GPIO> to <state>\r\n");
    am_util_stdio_printf("  read    read state from <GPIO>\r\n");
    am_util_stdio_printf("  help    show command details\r\n");
    am_util_stdio_printf("\r\n");
}

static bool gpio_is_reserved(uint32_t pin)
{
    for (int i = 0; i < MAX_RESERVED_PINS; i++) {
        if (gReservedPins[i] == pin)
            return true;
    }

    return false;
}

static uint32_t gpio_init_output(uint32_t pin)
{
    if (gpio_is_reserved(pin)) {
        return AM_HAL_STATUS_FAIL;
    }

    if (pin < MAX_GPIO_PINS) {
        am_hal_gpio_pinconfig(pin, g_AM_HAL_GPIO_OUTPUT);

        am_hal_gpio_state_write(pin, AM_HAL_GPIO_OUTPUT_CLEAR);
    }

    return AM_HAL_STATUS_SUCCESS;
}

static uint32_t gpio_init_input(uint32_t pin)
{
    if (gpio_is_reserved(pin)) {
        return AM_HAL_STATUS_FAIL;
    }

    if (pin < MAX_GPIO_PINS) {
        am_hal_gpio_pinconfig(pin, g_AM_HAL_GPIO_INPUT);
    }

    return AM_HAL_STATUS_SUCCESS;
}

static uint32_t gpio_deinit(uint32_t pin)
{
    if (gpio_is_reserved(pin)) {
        return AM_HAL_STATUS_FAIL;
    }

    if (pin < MAX_GPIO_PINS) {
        am_hal_gpio_pinconfig(pin, g_AM_HAL_GPIO_DISABLE);
    }

    return AM_HAL_STATUS_SUCCESS;
}

static void init(char *pui8OutBuffer, size_t argc, char **argv)
{
    if (argc < 4)
    {
        return;
    }

    if (strcmp(argv[3], "output") == 0)
    {
        char *end_ptr;
        uint32_t ui32Pin = strtol(argv[2], &end_ptr, 10);
        if (gpio_init_output(ui32Pin))
        {
            am_util_stdio_printf("error: GPIO%d is a reserved pin.\r\n", ui32Pin);
        }
    }
    else if (strcmp(argv[3], "input") == 0)
    {
        char *end_ptr;
        uint32_t ui32Pin = strtol(argv[2], &end_ptr, 10);
        if (gpio_init_input(ui32Pin))
        {
            am_util_stdio_printf("error: GPIO%d is a reserved pin.\r\n", ui32Pin);
        }
    }
}

static void deinit(char *pui8OutBuffer, size_t argc, char **argv)
{
    if (argc < 3)
    {
        return;
    }

    char *end_ptr;
    uint32_t ui32Pin = strtol(argv[2], &end_ptr, 10);
    if (gpio_deinit(ui32Pin))
    {
        am_util_stdio_printf("error: GPIO%d is a reserved pin.\r\n", ui32Pin);
    }
}

static void read(char *pui8OutBuffer, size_t argc, char **argv)
{
    if (argc < 3)
    {
        return;
    }

    char *end_ptr;
    uint32_t ui32Pin = strtol(argv[2], &end_ptr, 10);
    if (gpio_is_reserved(ui32Pin))
    {
        am_util_stdio_printf("error: GPIO%d is a reserved pin.\r\n", ui32Pin);
    }

    uint32_t ui32Value;
    am_hal_gpio_state_read(ui32Pin, AM_HAL_GPIO_INPUT_READ, &ui32Value);
    am_util_stdio_printf("GPIO%d: %d\r\n", ui32Pin, ui32Value);
}

static void write(char *pui8OutBuffer, size_t argc, char **argv)
{
    if (argc < 4)
    {
        return;
    }

    char *end_ptr;
    uint32_t ui32Pin = strtol(argv[2], &end_ptr, 10);
    if (gpio_is_reserved(ui32Pin))
    {
        am_util_stdio_printf("error: GPIO%d is a reserved pin.\r\n", ui32Pin);
        return;
    }
 
    uint32_t ui32State = strtol(argv[3], &end_ptr, 10);
    am_hal_gpio_state_write(ui32Pin, ui32State ? AM_HAL_GPIO_OUTPUT_SET : AM_HAL_GPIO_OUTPUT_CLEAR);
}

static void toggle(char *pui8OutBuffer, size_t argc, char **argv)
{
    if (argc < 3)
    {
        return;
    }

    char *end_ptr;
    uint32_t ui32Pin = strtol(argv[2], &end_ptr, 10);
 
    if (gpio_is_reserved(ui32Pin))
    {
        am_util_stdio_printf("error: GPIO%d is a reserved pin.\r\n", ui32Pin);
        return;
    }
 
    am_hal_gpio_state_write(ui32Pin, AM_HAL_GPIO_OUTPUT_TOGGLE);
}

portBASE_TYPE gpio_cli_entry(char *pui8OutBuffer, size_t ui32OutBufferLnegth,
                             const char *pui8Command)
{
    pui8OutBuffer[0] = 0;

    memset(argz, 0, COMMAND_LINE_BUFFER_MAX);
    strcpy(argz, pui8Command);
    FreeRTOS_CLIExtractParameters(argz, &argc, argv);

    if (strcmp(argv[1], "help") == 0)
    {
        help(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "init") == 0)
    {
        init(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "deinit") == 0)
    {
        deinit(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "read") == 0)
    {
        read(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "write") == 0)
    {
        write(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "toggle") == 0)
    {
        toggle(pui8OutBuffer, argc, argv);
    }
    else
    {
        help(pui8OutBuffer, argc, argv);
    }

    return pdFALSE;
}
