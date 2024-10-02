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
#include <string.h>

#include "lorawan.h"

SecureElementNvmData_t gsLoRaWANSecureElement = {.DevEui = {0},
                                                 .JoinEui = {0},
                                                 .Pin = {0},
                                                 .KeyList = {
                                                     {.KeyID = APP_KEY, .KeyValue = {0}},
                                                     {.KeyID = NWK_KEY, .KeyValue = {0}},
                                                     {.KeyID = J_S_INT_KEY, .KeyValue = {0}},
                                                     {.KeyID = J_S_ENC_KEY, .KeyValue = {0}},
                                                     {.KeyID = F_NWK_S_INT_KEY, .KeyValue = {0}},
                                                     {.KeyID = S_NWK_S_INT_KEY, .KeyValue = {0}},
                                                     {.KeyID = NWK_S_ENC_KEY, .KeyValue = {0}},
                                                     {.KeyID = APP_S_KEY, .KeyValue = {0}},
                                                     {.KeyID = MC_ROOT_KEY, .KeyValue = {0}},
                                                     {.KeyID = MC_KE_KEY, .KeyValue = {0}},
                                                     {.KeyID = MC_KEY_0, .KeyValue = {0}},
                                                     {.KeyID = MC_APP_S_KEY_0, .KeyValue = {0}},
                                                     {.KeyID = MC_NWK_S_KEY_0, .KeyValue = {0}},
                                                     {.KeyID = MC_KEY_1, .KeyValue = {0}},
                                                     {.KeyID = MC_APP_S_KEY_1, .KeyValue = {0}},
                                                     {.KeyID = MC_NWK_S_KEY_1, .KeyValue = {0}},
                                                     {.KeyID = MC_KEY_2, .KeyValue = {0}},
                                                     {.KeyID = MC_APP_S_KEY_2, .KeyValue = {0}},
                                                     {.KeyID = MC_NWK_S_KEY_2, .KeyValue = {0}},
                                                     {.KeyID = MC_KEY_3, .KeyValue = {0}},
                                                     {.KeyID = MC_APP_S_KEY_3, .KeyValue = {0}},
                                                     {.KeyID = MC_NWK_S_KEY_3, .KeyValue = {0}},
                                                     {.KeyID = SLOT_RAND_ZERO_KEY, .KeyValue = {0}},
                                                 }};

static uint8_t hex_char(const char ch)
{
    if ('0' <= ch && ch <= '9')
        return (uint8_t)(ch - '0');

    if ('a' <= ch && ch <= 'f')
        return (uint8_t)(ch - 'a' + 10);

    if ('A' <= ch && ch <= 'F')
        return (uint8_t)(ch - 'A' + 10);

    return 255;
}

static uint8_t hex_to_bin(const char *str, uint8_t *array)
{
    while (*str)
    {
        char ch = *str++;
        if ((ch == ' ') || (ch == '-') || (ch == ':'))
        {
            continue;
        }

        uint8_t n1 = hex_char(ch);
        uint8_t n2 = hex_char(*str++);

        if ((n1 == 255) || (n2 == 255))
        {
            return 0;
        }

        *array++ = (n1 << 4) + n2;
    }

    return 0;
}

void lorawan_key_set_by_str(lorawan_key_e eKey, const char *pcKey)
{
    switch (eKey)
    {
    case LORAWAN_KEY_DEV_EUI:
        hex_to_bin(pcKey, gsLoRaWANSecureElement.DevEui);
        return;

    case LORAWAN_KEY_JOIN_EUI:
        hex_to_bin(pcKey, gsLoRaWANSecureElement.JoinEui);
        return;

    default:
        break;
    }

    KeyIdentifier_t keyIdentifier;
    switch(eKey)
    {
    case LORAWAN_KEY_APP:
        keyIdentifier = APP_KEY;
        break;
    case LORAWAN_KEY_NWK:
        keyIdentifier = NWK_KEY;
        break;
    case LORAWAN_KEY_F_NWK_S_INT:
        keyIdentifier = F_NWK_S_INT_KEY;
        break;
    case LORAWAN_KEY_S_NWK_S_INT:
        keyIdentifier = S_NWK_S_INT_KEY;
        break;
    case LORAWAN_KEY_APP_S:
        keyIdentifier = APP_S_KEY;
        break;
    case LORAWAN_KEY_NWK_S_ENC:
        keyIdentifier = NWK_S_ENC_KEY;
        break;
    default:
        return;
    }

    for (int i = 0; i < NUM_OF_KEYS; i++)
    {
        if (gsLoRaWANSecureElement.KeyList[i].KeyID == keyIdentifier)
        {
            hex_to_bin(pcKey, gsLoRaWANSecureElement.KeyList[i].KeyValue);
            return;
        }
    }
}

void lorawan_key_set_by_bytes(lorawan_key_e eKey, const uint8_t *pui8Key)
{
    switch (eKey)
    {
    case LORAWAN_KEY_DEV_EUI:
        memcpy(gsLoRaWANSecureElement.DevEui, pui8Key, SE_EUI_SIZE);
        return;

    case LORAWAN_KEY_JOIN_EUI:
        memcpy(gsLoRaWANSecureElement.JoinEui, pui8Key, SE_EUI_SIZE);
        return;

    default:
        break;
    }

    KeyIdentifier_t keyIdentifier;
    switch(eKey)
    {
    case LORAWAN_KEY_APP:
        keyIdentifier = APP_KEY;
        break;
    case LORAWAN_KEY_NWK:
        keyIdentifier = NWK_KEY;
        break;
    case LORAWAN_KEY_F_NWK_S_INT:
        keyIdentifier = F_NWK_S_INT_KEY;
        break;
    case LORAWAN_KEY_S_NWK_S_INT:
        keyIdentifier = S_NWK_S_INT_KEY;
        break;
    case LORAWAN_KEY_APP_S:
        keyIdentifier = APP_S_KEY;
        break;
    case LORAWAN_KEY_NWK_S_ENC:
        keyIdentifier = NWK_S_ENC_KEY;
        break;
    default:
        return;
    }

    for (int i = 0; i < NUM_OF_KEYS; i++)
    {
        if (gsLoRaWANSecureElement.KeyList[i].KeyID == keyIdentifier)
        {
            memcpy(gsLoRaWANSecureElement.KeyList[i].KeyValue, pui8Key, SE_KEY_SIZE);
            return;
        }
    }
}

void lorawan_key_get(lorawan_key_e eKey, uint8_t *pui8Key)
{
    switch (eKey)
    {
    case LORAWAN_KEY_DEV_EUI:
        memcpy(pui8Key, gsLoRaWANSecureElement.DevEui, SE_EUI_SIZE);
        return;
        
    case LORAWAN_KEY_JOIN_EUI:
        memcpy(pui8Key, gsLoRaWANSecureElement.JoinEui, SE_EUI_SIZE);
        return;

    default:
        break;
    }

    KeyIdentifier_t keyIdentifier;
    switch(eKey)
    {
    case LORAWAN_KEY_APP:
        keyIdentifier = APP_KEY;
        break;
    case LORAWAN_KEY_NWK:
        keyIdentifier = NWK_KEY;
        break;
    case LORAWAN_KEY_F_NWK_S_INT:
        keyIdentifier = F_NWK_S_INT_KEY;
        break;
    case LORAWAN_KEY_S_NWK_S_INT:
        keyIdentifier = S_NWK_S_INT_KEY;
        break;
    case LORAWAN_KEY_APP_S:
        keyIdentifier = APP_S_KEY;
        break;
    case LORAWAN_KEY_NWK_S_ENC:
        keyIdentifier = NWK_S_ENC_KEY;
        break;
    default:
        return;
    }

    for (int i = 0; i < NUM_OF_KEYS; i++)
    {
        if (gsLoRaWANSecureElement.KeyList[i].KeyID == keyIdentifier)
        {
            memcpy(pui8Key, gsLoRaWANSecureElement.KeyList[i].KeyValue, SE_KEY_SIZE);
            return;
        }
    }
}