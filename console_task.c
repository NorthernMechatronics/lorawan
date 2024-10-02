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
#include <stddef.h>
#include <string.h>

#include <am_bsp.h>
#include <am_util.h>

#include <FreeRTOS.h>
#include <FreeRTOS_CLI.h>
#include <stream_buffer.h>
#include <task.h>

#include "nm_sdk_version.h"

#include "SEGGER_RTT.h"
#include "console_task.h"

#define CONSOLE_UART_INST 0

#define MAX_CMD_HIST_LEN (8)
#define MAX_INPUT_LEN    (256)

#define STREAM_BUFFER_SIZE 64

static console_output_e console_output;

static volatile StreamBufferHandle_t stream_buffer;

static uint8_t uart_buffer[32];
static am_hal_uart_transfer_t uart_transfer = {
    .ui32Direction = AM_HAL_UART_READ,
    .pui8Data = uart_buffer,
    .ui32NumBytes = 32,
    .ui32TimeoutMs = 0,
    .pui32BytesTransferred = 0,
};

static char cmd_buffer[MAX_INPUT_LEN];
static uint8_t cmd_size = 0;

static const char cmd_prompt[] = "> ";
static const char title_msg[] = "\r\n\r\n"
                                "Northern Mechatronics\r\n"
                                "NM1801xx Command Console\r\n\r\n";

static char cmd_hist[MAX_CMD_HIST_LEN][MAX_INPUT_LEN];
static uint8_t cmd_hist_len = 0;
static uint8_t cmd_hist_first = 0;
static uint8_t cmd_hist_last = 0;
static uint8_t cmd_hist_cur = 0;

static const char crlf[] = "\r\n";

static TaskHandle_t console_task_handle;

static void console_task(void *parameter);

static void console_cmd_hist_add(const char *cmd, size_t len)
{
    uint8_t next;

    if (cmd_hist_len < MAX_CMD_HIST_LEN)
    {
        next = cmd_hist_len;
        cmd_hist_len++;
    }
    else
    {
        next = (cmd_hist_last + 1) % MAX_CMD_HIST_LEN;
    }
    strncpy(cmd_hist[next], cmd, len);
    cmd_hist[next][len] = '\x0';
    cmd_hist_last = next;
    cmd_hist_cur = next;
    // wrap-around
    if (cmd_hist_last == cmd_hist_first && cmd_hist_len == MAX_CMD_HIST_LEN)
    {
        cmd_hist_first = (cmd_hist_first + 1) % MAX_CMD_HIST_LEN;
    }
}

static const char *console_cmd_hist_prev(void)
{
    const char *cmd;

    // Empty history buffer
    if (cmd_hist_len == 0)
    {
        return NULL;
    }

    if (cmd_hist_cur == cmd_hist_len)
    {
        cmd = NULL;
    }
    else
    {
        cmd = cmd_hist[cmd_hist_cur];
    }
    cmd_hist_cur = (cmd_hist_cur + cmd_hist_len) % (cmd_hist_len + 1);

    return cmd;
}

static const char *console_cmd_hist_next(void)
{
    // Empty history buffer
    if (cmd_hist_len == 0)
    {
        return NULL;
    }

    cmd_hist_cur = (cmd_hist_cur + 1) % (cmd_hist_len + 1);
    if (cmd_hist_cur == cmd_hist_len)
    {
        return NULL;
    }

    return cmd_hist[cmd_hist_cur];
}

static void console_clear_line(uint8_t pos)
{
    if (pos > 0)
    {
        // ANSI escape code CUB
        am_util_stdio_printf("\e[%dD\e[0K", pos);
    }
}

static char console_read()
{
    uint8_t ch;

    if (console_output == CONSOLE_OUTPUT_RTT)
    {
        ch = SEGGER_RTT_WaitKey();
    }
    else if (console_output == CONSOLE_OUTPUT_UART)
    {
        xStreamBufferReceive(stream_buffer, &ch, 1, portMAX_DELAY);
    }

    return ch;
}

static void console_rtt_print(char *str)
{
    SEGGER_RTT_WriteString(0, str);
}

static void console_task_setup(void)
{
    if (console_output == CONSOLE_OUTPUT_RTT)
    {
        SEGGER_RTT_ConfigUpBuffer(0, NULL, NULL, 0, SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
        am_util_stdio_printf_init(console_rtt_print);
    }
    else if (console_output == CONSOLE_OUTPUT_UART)
    {
        am_bsp_buffered_uart_printf_enable();
        NVIC_SetPriority((IRQn_Type)(UART0_IRQn + AM_BSP_UART_PRINT_INST),
                        NVIC_configKERNEL_INTERRUPT_PRIORITY);

        memset(cmd_hist, 0, MAX_CMD_HIST_LEN * MAX_INPUT_LEN);

        stream_buffer = xStreamBufferCreate(STREAM_BUFFER_SIZE, 1);
    }
}

static void console_task(void *parameter)
{
    char ch;
    char *out_str;
    portBASE_TYPE ret;

    out_str = FreeRTOS_CLIGetOutputBuffer();

    while (1)
    {
        ch = console_read();

        switch ((uint8_t)ch)
        {
        case '\e':
            ch = console_read();
            ch = console_read();
            if (ch == 'A')
            {
                const char *cmd = console_cmd_hist_prev();

                console_clear_line(cmd_size);
                if (cmd != NULL)
                {
                    strcpy(cmd_buffer, cmd);
                    am_util_stdio_printf(cmd_buffer);
                    cmd_size = strlen(cmd_buffer);
                }
                else
                {
                    cmd_size = 0;
                }
            }
            else if (ch == 'B')
            {
                const char *cmd = console_cmd_hist_next();

                console_clear_line(cmd_size);
                if (cmd != NULL)
                {
                    strcpy(cmd_buffer, cmd);
                    am_util_stdio_printf(cmd_buffer);
                    cmd_size = strlen(cmd_buffer);
                }
                else
                {
                    cmd_size = 0;
                }
            }
            break;

        case '\b':
        case '\x7f':
            if (cmd_size > 0)
            {
                console_clear_line(cmd_size--);

                cmd_buffer[cmd_size] = '\0';
                am_util_stdio_printf(cmd_buffer);
            }
            break;

        case '\r':
        case '\n':
            am_util_stdio_printf(crlf);
            if (cmd_size == 0)
            {
                console_print_prompt();
                cmd_hist_cur = cmd_hist_last;
                break;
            }

            do
            {
                ret = FreeRTOS_CLIProcessCommand(
                    cmd_buffer, out_str, configCOMMAND_INT_MAX_OUTPUT_SIZE);
                am_util_stdio_printf(out_str);
            } while (ret != pdFALSE);

            am_util_stdio_printf(crlf);
            console_print_prompt();

            console_cmd_hist_add(cmd_buffer, cmd_size);
            cmd_size = 0;
            memset(cmd_buffer, 0x00, MAX_INPUT_LEN);
            break;

        default:
            am_util_stdio_printf("%c", ch);

            if ((ch >= ' ') && (ch <= '~'))
            {
                if (cmd_size < MAX_INPUT_LEN)
                {
                    cmd_buffer[cmd_size] = ch;
                    cmd_size++;
                }
            }
            break;
        }
    }
}

void console_task_create(uint32_t priority, console_output_e output)
{
    console_output = output;
    console_task_setup();
    am_util_stdio_printf(title_msg);
    am_util_stdio_printf("SDK version %s\r\n\r\n", nm_sdk_version);
    am_util_stdio_printf("Type `help` for a list of commands\r\n\r\n");
    console_print_prompt();

    xTaskCreate(console_task, "console", 512, 0, priority, &console_task_handle);
}

void console_print_prompt()
{
    uint32_t ticks = xTaskGetTickCount();

    uint32_t unit = ticks / configTICK_RATE_HZ;
    uint32_t subseconds = ticks % configTICK_RATE_HZ;
    uint32_t seconds = unit % 60;
    uint32_t minutes = unit / 60;
    uint32_t hours = minutes / 60;

    am_util_stdio_printf("%02d:%02d:%02d.%03d ", hours, minutes, seconds, subseconds);
    am_util_stdio_printf(cmd_prompt);
}

void am_uart_isr()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t received = 0;

    uart_transfer.pui32BytesTransferred = &received;

    am_bsp_buffered_uart_service();
    am_bsp_com_uart_transfer(&uart_transfer);
    if (received > 0)
    {
        xStreamBufferSendFromISR(
            stream_buffer, (void *)uart_buffer, received, &xHigherPriorityTaskWoken);
    }

    portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
}
