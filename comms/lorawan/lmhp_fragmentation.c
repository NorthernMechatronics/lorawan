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
#include <stdint.h>
#include <string.h>

#include <am_mcu_apollo.h>
#include <am_util.h>

#include "ota_config.h"

#include "lorawan.h"
#include "lorawan_task.h"

#define AUTH_REQ_BUFFER_SIZE (5)
static uint8_t auth_req_buffer[AUTH_REQ_BUFFER_SIZE];
static uint8_t frag_write_status[FRAG_MAX_NB];

static void on_frag_progress(uint16_t ui16Counter, uint16_t ui16Blocks, uint8_t ui8Size, uint16_t ui16Lost)
{
    if (lorawan_tracing_enabled)
    {
        am_util_stdio_printf("\r\n");
        am_util_stdio_printf("###### =========== FRAG_DECODER ============ ######\r\n");
        am_util_stdio_printf("######               PROGRESS                ######\r\n");
        am_util_stdio_printf("###### ===================================== ######\r\n");
        am_util_stdio_printf("RECEIVED    : %5d / %5d Fragments\r\n", ui16Counter, ui16Blocks);
        am_util_stdio_printf("              %5d / %5d Bytes\r\n", ui16Counter * ui8Size, ui16Blocks * ui8Size);
        am_util_stdio_printf("LOST        :       %7d Fragments\r\n\r\n", ui16Lost);
    }
}

static void on_frag_done(int32_t ui32Status, uint32_t ui32Size)
{
    uint32_t ui32Crc = Crc32((uint8_t *)OTA_FLASH_ADDRESS, ui32Size);

    auth_req_buffer[0] = 0x05;
    auth_req_buffer[1] = ui32Crc & 0x000000FF;
    auth_req_buffer[2] = (ui32Crc >> 8) & 0x000000FF;
    auth_req_buffer[3] = (ui32Crc >> 16) & 0x000000FF;
    auth_req_buffer[4] = (ui32Crc >> 24) & 0x000000FF;

    lorawan_transmit(
        FRAGMENTATION_PORT, LORAMAC_HANDLER_UNCONFIRMED_MSG, AUTH_REQ_BUFFER_SIZE, auth_req_buffer);

    if (lorawan_tracing_enabled)
    {
        am_util_stdio_printf("\r\n");
        am_util_stdio_printf("###### =========== FRAG_DECODER ============ ######\r\n");
        am_util_stdio_printf("######               FINISHED                ######\r\n");
        am_util_stdio_printf("###### ===================================== ######\r\n");
        am_util_stdio_printf("STATUS : %ld\r\n", ui32Status);
        am_util_stdio_printf("SIZE   : %ld\r\n", ui32Size);
        am_util_stdio_printf("CRC    : %08lX\n\n", ui32Crc);
    }
}

static int8_t frag_decoder_write(uint32_t ui32Offset, uint8_t *pui8Data, uint32_t ui32Size)
{
    uint32_t *pui32Destination = (uint32_t *)(OTA_FLASH_ADDRESS + ui32Offset);
    uint32_t pui32Source[64];
    uint32_t pui32Length = ui32Size >> 2;

    if (lorawan_tracing_enabled)
    {
        am_util_stdio_printf(
            "\r\nDecoder Write: 0x%x, 0x%x, %d\r\n", (uint32_t)pui32Destination, (uint32_t)pui32Source, pui32Length);
    }

    memcpy(pui32Source, pui8Data, ui32Size);

    AM_CRITICAL_BEGIN
    am_hal_flash_program_main(AM_HAL_FLASH_PROGRAM_KEY, pui32Source, pui32Destination, pui32Length);
    AM_CRITICAL_END

    return 0;
}

static int8_t frag_decoder_read(uint32_t ui32Offset, uint8_t *pui8Data, uint32_t ui32Size)
{
    uint8_t *pui8UnfragmentedData = (uint8_t *)(OTA_FLASH_ADDRESS);
    for (uint32_t i = 0; i < ui32Size; i++)
    {
        pui8Data[i] = pui8UnfragmentedData[ui32Offset + i];
    }
    return 0;
}

static int8_t frag_decoder_erase(uint32_t ui32Offset, uint32_t ui32Block, uint32_t ui32Size)
{
    uint32_t ui32TotalSize = ui32Block * ui32Size;
    uint32_t ui32TotalPage = (ui32TotalSize >> 13) + 1;
    uint32_t ui32Address = OTA_FLASH_ADDRESS;

    memset(frag_write_status, 1, FRAG_MAX_NB);
    memset(frag_write_status, 0, ui32Block);

    if (lorawan_tracing_enabled)
    {
        am_util_stdio_printf("\r\nErasing %d pages at 0x%x\r\n", ui32TotalPage, ui32Address);
    }

    for (int i = 0; i < ui32TotalPage; i++)
    {
        ui32Address += AM_HAL_FLASH_PAGE_SIZE;

        if (lorawan_tracing_enabled)
        {
            am_util_stdio_printf("Instance: %d, Page: %d\r\n",
                                AM_HAL_FLASH_ADDR2INST(ui32Address),
                                AM_HAL_FLASH_ADDR2PAGE(ui32Address));
        }

        AM_CRITICAL_BEGIN
        am_hal_flash_page_erase(AM_HAL_FLASH_PROGRAM_KEY,
                                AM_HAL_FLASH_ADDR2INST(ui32Address),
                                AM_HAL_FLASH_ADDR2PAGE(ui32Address));
        AM_CRITICAL_END
    }

    return 0;
}

void lmhp_fragmentation_setup(LmhpFragmentationParams_t *psParameters)
{
    psParameters->OnProgress = on_frag_progress;
    psParameters->OnDone = on_frag_done;
    psParameters->DecoderCallbacks.FragDecoderWrite = frag_decoder_write;
    psParameters->DecoderCallbacks.FragDecoderRead = frag_decoder_read;
    psParameters->DecoderCallbacks.FragDecoderErase = frag_decoder_erase;
}