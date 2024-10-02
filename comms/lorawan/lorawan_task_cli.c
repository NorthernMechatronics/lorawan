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
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <am_bsp.h>
#include <am_mcu_apollo.h>
#include <am_util.h>

#include <FreeRTOS.h>
#include <FreeRTOS_CLI.h>
#include <timers.h>

#include <LmHandler.h>
#include <LmhpRemoteMcastSetup.h>
#include <LoRaMacTypes.h>
#include <board.h>

#include <eeprom_emulation.h>
#include <lorawan_eeprom_config.h>

#include "lorawan_config.h"

#include "lorawan.h"
#include "lorawan_task.h"
#include "lorawan_task_cli.h"

#define COMMAND_LINE_BUFFER_MAX     (128)

static portBASE_TYPE
lorawan_task_cli_entry(char *pui8OutBuffer, size_t ui32OutBufferLength, const char *pui8Command);

CLI_Command_Definition_t lorawan_task_cli_definition = {
    (const char *const) "lorawan",
    (const char *const) "lorawan:  LoRaWAN Application Layer Commands.\r\n",
    lorawan_task_cli_entry,
    -1};

static size_t argc;
static char *argv[8];
static char argz[COMMAND_LINE_BUFFER_MAX];

#define LM_BUFFER_SIZE 242
static uint8_t lorawan_cli_transmit_buffer[LM_BUFFER_SIZE];
static uint32_t lorawan_default_port = 1;
static TimerHandle_t periodic_transmit_timer = NULL;

static void print_hex_array(char *pui8OutBuffer, uint8_t *array, uint32_t length, bool space)
{
    for (int i = 0; i < length; i++)
    {
        if (((i % 16) == 0) && (i > 0))
        {
            am_util_stdio_printf("\n\r  ");
        }
        am_util_stdio_printf("%02X", array[i]);

        if (space)
        {
            am_util_stdio_printf(" ");
        }
    }
}

static void periodic_transmit_callback(TimerHandle_t handle)
{
    uint32_t ui32Count = (uint32_t)pvTimerGetTimerID(handle);
    ui32Count++;
    vTimerSetTimerID(handle, (void *)ui32Count);

    am_util_stdio_sprintf((char *)lorawan_cli_transmit_buffer, "%d", ui32Count);
    uint32_t length = strlen((char *)lorawan_cli_transmit_buffer);

    lorawan_transmit(
        lorawan_default_port, LORAMAC_HANDLER_UNCONFIRMED_MSG, length, lorawan_cli_transmit_buffer);
}

void lorawan_task_cli_register()
{
    FreeRTOS_CLIRegisterCommand(&lorawan_task_cli_definition);
    argc = 0;
}

static void convert_hex_string(const char *in, size_t inlen, uint8_t *out, size_t *outlen)
{
    size_t n = 0;
    char cNum[3];
    *outlen = 0;
    while (n < inlen)
    {
        switch (in[n])
        {
        case '\\':
            n++;
            switch (in[n])
            {
            case 'x':
                n++;
                memset(cNum, 0, 3);
                memcpy(cNum, &in[n], 2);
                n++;
                out[*outlen] = strtol(cNum, NULL, 16);
                break;
            }
            break;
        default:
            out[*outlen] = in[n];
            break;
        }
        *outlen = *outlen + 1;
        n++;
    }
}

static void lorawan_task_cli_help(char *pui8OutBuffer, size_t argc, char **argv)
{
    am_util_stdio_printf("\r\nusage: lorawan <command>\r\n");
    am_util_stdio_printf("\r\n");
    am_util_stdio_printf("supported commands are:\r\n");
    am_util_stdio_printf("  start      start the LoRaWAN stack\r\n");
    am_util_stdio_printf("  stop       stop the LoRaWAN stack\r\n");
    am_util_stdio_printf("\r\n");
    am_util_stdio_printf("  class      <get|set> LoRaWAN class\r\n");
    am_util_stdio_printf("  clear      clear and reformat eeprom\r\n");
    am_util_stdio_printf("  datetime   <get|set|sync> network time\r\n");
    am_util_stdio_printf("  join       initiate a join\r\n");
    am_util_stdio_printf("  keys       display security keys\r\n");
    am_util_stdio_printf("  periodic   <start|stop> [period]\r\n");
    am_util_stdio_printf("             periodically transmit an incrementing counter\r\n");
    am_util_stdio_printf("  port       <start|stop> manual SPI port control\r\n");
    am_util_stdio_printf("  send       [port] [ack] <payload>\r\n");
    am_util_stdio_printf("             transmit a packet\r\n");
    am_util_stdio_printf("  status     display stack status\r\n");
    am_util_stdio_printf("  trace      <enable|disable> debug messages\r\n");
}

static void lorawan_task_cli_class(char *pui8OutBuffer, size_t argc, char **argv)
{
    lorawan_class_e cls;

    if (argc == 3)
    {
        if (strcmp(argv[2], "get") == 0)
        {
            am_util_stdio_printf("\n\rCurrent Class: ");

            lorawan_class_get(&cls);
            switch (cls)
            {
            case LORAWAN_CLASS_A:
                am_util_stdio_printf("A");
                break;
            case LORAWAN_CLASS_B:
                am_util_stdio_printf("B");
                break;
            case LORAWAN_CLASS_C:
                am_util_stdio_printf("C");
                break;
            }
            am_util_stdio_printf("\n\r");
        }
    }
    else if (argc == 4)
    {
        if (strcmp(argv[2], "set") == 0)
        {
            switch (argv[3][0])
            {
            case 'a':
            case 'A':
                cls = LORAWAN_CLASS_A;
                break;
            case 'b':
            case 'B':
                cls = LORAWAN_CLASS_B;
                break;
            case 'c':
            case 'C':
                cls = LORAWAN_CLASS_C;
                break;
            default:
                am_util_stdio_printf("\n\rUnknown class requested.\n\r");
                return;
            }

            lorawan_class_set(cls);
        }
    }
}

static void lorawan_task_cli_datetime(char *pui8OutBuffer, size_t argc, char **argv)
{
    if (argc == 2)
    {
        SysTime_t timestamp = SysTimeGet();
        struct tm localtime;

        SysTimeLocalTime(timestamp.Seconds, &localtime);

        am_util_stdio_printf(
            "\n\rUnix timestamp: %d\n\rStack Time: %02d/%02d/%04d %02d:%02d:%02d (UTC0)\n\r",
            timestamp.Seconds,
            localtime.tm_mon + 1,
            localtime.tm_mday,
            localtime.tm_year + 1900,
            localtime.tm_hour,
            localtime.tm_min,
            localtime.tm_sec);

        return;
    }

    lorawan_command_t command;
    if (argc >= 3)
    {
        if (strcmp(argv[2], "sync") == 0)
        {
            if (argc == 3)
            {
                command.eCommand = LORAWAN_SYNC_MAC;
            }
            else
            {
                command.eCommand = LORAWAN_SYNC_APP;
            }
            lorawan_send_command(&command);
        }
    }
}

static void lorawan_task_cli_port(char *pui8OutBuffer, size_t argc, char **argv)
{
    if (argc < 3)
    {
        return;
    }

    if (strcmp(argv[2], "stop") == 0)
    {
        BoardDeInitMcu();
    }
    else if (strcmp(argv[2], "start") == 0)
    {
        BoardInitMcu();
    }
}

static void lorawan_task_cli_trace(char *pui8OutBuffer, size_t argc, char **argv)
{
    if (argc < 3)
    {
        return;
    }

    if (strcmp(argv[2], "enable") == 0)
    {
        lorawan_tracing_set(1);
    }
    else if (strcmp(argv[2], "disable") == 0)
    {
        lorawan_tracing_set(0);
    }
}

static void lorawan_task_cli_keys(char *pui8OutBuffer, size_t argc, char **argv)
{
    uint8_t dev_eui[SE_EUI_SIZE];
    uint8_t join_eui[SE_EUI_SIZE];
    uint8_t app_key[SE_KEY_SIZE];
    uint8_t nwk_key[SE_KEY_SIZE];

    lorawan_key_get(LORAWAN_KEY_DEV_EUI, dev_eui);
    lorawan_key_get(LORAWAN_KEY_JOIN_EUI, join_eui);

    lorawan_key_get(LORAWAN_KEY_APP, app_key);
    lorawan_key_get(LORAWAN_KEY_NWK, nwk_key);

    am_util_stdio_printf("\n\r");

    am_util_stdio_printf("Device EUI  : ");
    print_hex_array(pui8OutBuffer, dev_eui, SE_EUI_SIZE, false);
    am_util_stdio_printf("\n\r");

    am_util_stdio_printf("Join EUI    : ");
    print_hex_array(pui8OutBuffer, join_eui, SE_EUI_SIZE, false);
    am_util_stdio_printf("\n\r");

    am_util_stdio_printf("App Key     : ");
    print_hex_array(pui8OutBuffer, app_key, SE_KEY_SIZE, false);
    am_util_stdio_printf("\n\r");

    am_util_stdio_printf("Network Key : ");
    print_hex_array(pui8OutBuffer, nwk_key, SE_KEY_SIZE, false);
    am_util_stdio_printf("\n\r");

    am_util_stdio_printf("\n\r");
}

static void lorawan_task_cli_periodic(char *pui8OutBuffer, size_t argc, char **argv)
{
    uint32_t ui32Period;
    if (argc < 3)
    {
        return;
    }

    if (strcmp(argv[2], "stop") == 0)
    {
        if (periodic_transmit_timer)
        {
            xTimerStop(periodic_transmit_timer, portMAX_DELAY);
            xTimerDelete(periodic_transmit_timer, portMAX_DELAY);
            periodic_transmit_timer = NULL;
        }
    }
    else if (strcmp(argv[2], "start") == 0)
    {
        if (argc == 3)
        {
            ui32Period = 300;
        }
        else
        {
            ui32Period = strtol(argv[3], NULL, 10);
        }

        if (periodic_transmit_timer == NULL)
        {
            periodic_transmit_timer = xTimerCreate("lorawan periodic",
                                                   pdMS_TO_TICKS(ui32Period * 1000),
                                                   pdTRUE,
                                                   (void *)0,
                                                   periodic_transmit_callback);
            xTimerStart(periodic_transmit_timer, portMAX_DELAY);
        }
        else
        {
            xTimerChangePeriod(periodic_transmit_timer, pdMS_TO_TICKS(ui32Period), portMAX_DELAY);
        }
        periodic_transmit_callback(periodic_transmit_timer);
    }
}

static void lorawan_task_cli_send(char *pui8OutBuffer, size_t argc, char **argv)
{
    uint32_t port = lorawan_default_port;
    uint32_t ack = LORAMAC_HANDLER_UNCONFIRMED_MSG;

    size_t length;
    convert_hex_string(argv[argc - 1], strlen(argv[argc - 1]), lorawan_cli_transmit_buffer, &length);
    lorawan_cli_transmit_buffer[length] = 0;

    if (argc == 5)
    {
        port = strtol(argv[2], NULL, 10);
        ack = strtol(argv[3], NULL, 10) ? LORAMAC_HANDLER_CONFIRMED_MSG : LORAMAC_HANDLER_UNCONFIRMED_MSG;
    }
    else if (argc == 4)
    {
        port = strtol(argv[2], NULL, 10);
    }

    lorawan_transmit(port, ack, length, lorawan_cli_transmit_buffer);
}

static void lorawan_task_cli_status(char *pui8OutBuffer, size_t argc, char **argv)
{
    lorawan_class_e cls;
    lorawan_class_get(&cls);

    am_util_stdio_printf("\n\r");
    am_util_stdio_printf("Current Class: ");
    switch (cls)
    {
    case LORAWAN_CLASS_A:
        am_util_stdio_printf("A");
        break;
    case LORAWAN_CLASS_B:
        am_util_stdio_printf("B");
        break;
    case LORAWAN_CLASS_C:
        am_util_stdio_printf("C");
        break;
    }
    am_util_stdio_printf("\n\r");

    bool status = LmhpRemoteMcastSessionStateStarted();
    am_util_stdio_printf("Multicast Session: ");
    if (status == true)
    {
        am_util_stdio_printf("in progress\n\r");
        uint32_t remainingTime = LmhpRemoteMcastSessionRemainingTime();
        am_util_stdio_printf("Remaining Time: ");
        am_util_stdio_printf("%u (ms) \n\r", remainingTime);
    }
    else
    {
        am_util_stdio_printf("none\n\r");
    }
 }

static portBASE_TYPE
lorawan_task_cli_entry(char *pui8OutBuffer, size_t ui32OutBufferLength, const char *pui8Command)
{
    pui8OutBuffer[0] = 0;

    memset(argz, 0, COMMAND_LINE_BUFFER_MAX);
    strcpy(argz, pui8Command);
    FreeRTOS_CLIExtractParameters(argz, &argc, argv);

    if (strcmp(argv[1], "help") == 0)
    {
        lorawan_task_cli_help(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "start") == 0)
    {
        lorawan_command_t command;
        command.eCommand = LORAWAN_START;
        lorawan_send_command(&command);
    }
    else if (strcmp(argv[1], "stop") == 0)
    {
        lorawan_command_t command;
        command.eCommand = LORAWAN_STOP;
        lorawan_send_command(&command);
    }
    else if (strcmp(argv[1], "class") == 0)
    {
        lorawan_task_cli_class(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "clear") == 0)
    {
        eeprom_format(&lorawan_eeprom_handle);
    }
    else if (strcmp(argv[1], "datetime") == 0)
    {
        lorawan_task_cli_datetime(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "join") == 0)
    {
        lorawan_join();
    }
    else if (strcmp(argv[1], "keys") == 0)
    {
        lorawan_task_cli_keys(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "periodic") == 0)
    {
        lorawan_task_cli_periodic(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "send") == 0)
    {
        lorawan_task_cli_send(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "port") == 0)
    {
        lorawan_task_cli_port(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "trace") == 0)
    {
        lorawan_task_cli_trace(pui8OutBuffer, argc, argv);
    }
    else if (strcmp(argv[1], "status") == 0)
    {
        lorawan_task_cli_status(pui8OutBuffer, argc, argv);
    }
    else
    {
        lorawan_task_cli_help(pui8OutBuffer, argc, argv);
    }

    return pdFALSE;
}
