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
#ifndef _LORAWAN_H_
#define _LORAWAN_H_

#include <LmHandler.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief State types for the LoRaWAN stack:
 *   LORAWAN_STACK_STARTED
 *   LORAWAN_STACK_STOPPED
 * 
 */
typedef enum
{
    LORAWAN_STACK_STARTED,
    LORAWAN_STACK_STOPPED,
} lorawan_stack_state_e;

/**
 * @brief LoRaWAN event types.
 * 
 */
typedef enum
{
    LORAWAN_EVENT_MAC_PROCESS = 0,
    LORAWAN_EVENT_NVM_DATA_CHANGE,
    LORAWAN_EVENT_NETWORK_PARAMETERS_CHANGE,
    LORAWAN_EVENT_MAC_MCPS_REQUEST,
    LORAWAN_EVENT_MAC_MLME_REQUEST,
    LORAWAN_EVENT_JOIN_REQUEST,
    LORAWAN_EVENT_TX_DATA,
    LORAWAN_EVENT_RX_DATA,
    LORAWAN_EVENT_CLASS_CHANGE,
    LORAWAN_EVENT_BEACON_STATUS_CHANGE,
    LORAWAN_EVENT_SYS_TIME_UPDATE,
    LORAWAN_EVENT_SLEEP,
    LORAWAN_EVENT_WAKE,
    LORAWAN_EVENTS
} lorawan_event_e;

/**
 * @brief LoRaWAN activation types.
 * 
 */
typedef enum
{
    LORAWAN_ACTIVATION_OTAA, ///< Activation over the air
    LORAWAN_ACTIVATION_ABP   ///< Activation by personalization
} lorawan_activation_type_e;

/**
 * @brief LoRaWAN supported classes
 * 
 */
typedef enum
{
    LORAWAN_CLASS_A,
    LORAWAN_CLASS_B,
    LORAWAN_CLASS_C
} lorawan_class_e;

typedef enum
{
    LORAWAN_KEY_DEV_EUI,
    LORAWAN_KEY_JOIN_EUI,
    LORAWAN_KEY_APP,
    LORAWAN_KEY_NWK,
    LORAWAN_KEY_F_NWK_S_INT,
    LORAWAN_KEY_S_NWK_S_INT,
    LORAWAN_KEY_APP_S,
    LORAWAN_KEY_NWK_S_ENC,
} lorawan_key_e;

/**
 * @brief LoRaWAN supported regions.
 * 
 */
typedef enum
{
    LORAWAN_REGION_AS923,
    LORAWAN_REGION_AU915,
    LORAWAN_REGION_CN470,
    LORAWAN_REGION_CN779,
    LORAWAN_REGION_EU433,
    LORAWAN_REGION_EU868,
    LORAWAN_REGION_KR920,
    LORAWAN_REGION_IN865,
    LORAWAN_REGION_US915,
    LORAWAN_REGION_RU864
} lorawan_region_e;

/**
 * @brief LoRaWAN datarates.
 * 
 */
typedef enum
{
    LORAWAN_DATARATE_0,
    LORAWAN_DATARATE_1,
    LORAWAN_DATARATE_2,
    LORAWAN_DATARATE_3,
    LORAWAN_DATARATE_4,
    LORAWAN_DATARATE_5,
    LORAWAN_DATARATE_6,
    LORAWAN_DATARATE_7,
    LORAWAN_DATARATE_8,
    LORAWAN_DATARATE_9,
    LORAWAN_DATARATE_10,
    LORAWAN_DATARATE_11,
    LORAWAN_DATARATE_12,
    LORAWAN_DATARATE_13,
    LORAWAN_DATARATE_14,
    LORAWAN_DATARATE_15,
} lorawan_datarate_e;

/**
 * @brief LoRaWAN ABP activation parameters.
 * 
 */
typedef struct
{
    uint32_t abp_server_version;
    uint32_t abp_network_id;
    uint32_t abp_device_address;
} lorawan_activation_parameters_t;

/**
 * @brief LoRaWAN event callback function generic prototype.
 * 
 */
typedef void* lorawan_event_callback_t;

/**
 * @brief Configure the LoRaWAN network the device will be operating in.
 * 
 * @param sRegion Valid regions are:
 *  - LORAWAN_REGION_AS923
 *  - LORAWAN_REGION_AU915
 *  - LORAWAN_REGION_CN470
 *  - LORAWAN_REGION_CN779
 *  - LORAWAN_REGION_EU433
 *  - LORAWAN_REGION_EU868
 *  - LORAWAN_REGION_KR920
 *  - LORAWAN_REGION_IN865
 *  - LORAWAN_REGION_US915
 *  - LORAWAN_REGION_RU864
 *
 * @param ui8DataRate Valid data rate values are:
 *  - LORAWAN_DATARATE_0
 *  - LORAWAN_DATARATE_1
 *  - LORAWAN_DATARATE_2
 *  - LORAWAN_DATARATE_3
 *  - LORAWAN_DATARATE_4
 *  - LORAWAN_DATARATE_5
 *  - LORAWAN_DATARATE_6
 *  - LORAWAN_DATARATE_7
 *  - LORAWAN_DATARATE_8
 *  - LORAWAN_DATARATE_9
 *  - LORAWAN_DATARATE_10
 *  - LORAWAN_DATARATE_11
 *  - LORAWAN_DATARATE_12
 *  - LORAWAN_DATARATE_13
 *  - LORAWAN_DATARATE_14   currently not used in any regions
 *  - LORAWAN_DATARATE_15   currently not used in any regions
 * 
 * @param i8ADR Set to 1 to enable adaptive data rate.
 *                 Set to 0 to disable.
 * 
 * @param i8PublicNetwork Set to 1 for public network.
 *                         Set to 0 for private network
 * 
 * @remarks Refer to the regional parameters for the region specific 
 * spreading factor and bandwidth definition for each data rate.
 */
extern void lorawan_network_config(lorawan_region_e eRegion,
                                   lorawan_datarate_e eDataRate,
                                   int8_t i8ADR,
                                   int8_t i8PublicNetwork);

/**
 * @brief Configure the device activation type.
 * 
 * @param sType Valid activation types are:
 *  - LORAWAN_ACTIVATION_OTAA
 *  - LORAWAN_ACTIVATION_ABP
 * 
 * @param psParams Activation parameters. This is used on in activation by personalization.
 *   Set to NULL for OTAA.
 */
extern void lorawan_activation_config(lorawan_activation_type_e sType,
                                      lorawan_activation_parameters_t *psParams);

/**
 * @brief Set the state of the LoRaWAN stack.
 * 
 * @param eState Valid values are:
 *  - LORAWAN_STACK_STARTED
 *  - LORAWAN_STACK_STOPPED
 */
extern void lorawan_stack_state_set(lorawan_stack_state_e eState);

/**
 * @brief Return the current state of the LoRaWAN stack.
 * 
 * @return lorawan_stack_state_e 
 */
extern void lorawan_stack_state_get(lorawan_stack_state_e *peState);

/**
 * @brief Initiate a join request
 * 
 */
extern void lorawan_join();

/**
 * @brief Get the current join state
 * 
 * @return uint32_t 
 */
extern uint32_t lorawan_get_join_state();

/**
 * @brief Set the device class
 * 
 * @param eDeviceClass  device class to set to
 */
extern void lorawan_class_set(lorawan_class_e eDeviceClass);

/**
 * @brief Retrieve the device class
 * 
 * @param peDeviceClass 
 */
extern void lorawan_class_get(lorawan_class_e *peDeviceClass);

/**
 * @brief Request a MAC layer time sync
 * 
 */
extern void lorawan_request_time_sync();

/**
 * @brief Transmit a packet.
 * 
 * @param ui32Port 
 * @param ui32Ack 
 * @param ui32Length 
 * @param pui8Data 
 */
extern void
lorawan_transmit(uint32_t ui32Port, uint32_t ui32Ack, uint32_t ui32Length, uint8_t *pui8Data);

/**
 * @brief Set a key by a string.
 * 
 * @param eKey  Valid values are:
 *  LORAWAN_KEY_DEV_EUI,
 *  LORAWAN_KEY_JOIN_EUI,
 *  LORAWAN_KEY_APP,
 *  LORAWAN_KEY_NWK,
 *  LORAWAN_KEY_F_NWK_S_INT,
 *  LORAWAN_KEY_S_NWK_S_INT,
 *  LORAWAN_KEY_APP_S,
 *  LORAWAN_KEY_NWK_S_ENC,
 *
 * @param pcKey  String representation of hexadecimal key values.  For example
 *      "01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F"
 * 
 * @remarks hexadecimal digit pairs can be separated by:
 *   ' ', '-', ':'
 */
extern void lorawan_key_set_by_str(lorawan_key_e eKey, const char *pcKey);

/**
 * @brief Retrieve the key value as an array of bytes.
 * 
 * @param eKey  Valid values are:
 *  LORAWAN_KEY_DEV_EUI,
 *  LORAWAN_KEY_JOIN_EUI,
 *  LORAWAN_KEY_APP,
 *  LORAWAN_KEY_NWK,
 *  LORAWAN_KEY_F_NWK_S_INT,
 *  LORAWAN_KEY_S_NWK_S_INT,
 *  LORAWAN_KEY_APP_S,
 *  LORAWAN_KEY_NWK_S_ENC,
 * 
 * @param pui8Key  Pointer to an array of bytes.
 */
extern void lorawan_key_set_by_bytes(lorawan_key_e eKey, const uint8_t *pui8Key);

/**
 * @brief Retrieve the key value as an array of bytes.
 * 
 * @param eKey  Valid values are:
 *  LORAWAN_KEY_DEV_EUI,
 *  LORAWAN_KEY_JOIN_EUI,
 *  LORAWAN_KEY_APP,
 *  LORAWAN_KEY_NWK,
 *  LORAWAN_KEY_F_NWK_S_INT,
 *  LORAWAN_KEY_S_NWK_S_INT,
 *  LORAWAN_KEY_APP_S,
 *  LORAWAN_KEY_NWK_S_ENC,
 * 
 * @param pui8Key  Pointer to an array of bytes.
 */
extern void lorawan_key_get(lorawan_key_e eKey, uint8_t *pui8Key);

/**
 * @brief Register a callback to a LoRaWAN event.
 * 
 * @param eEvent  LoRaWAN stack events.
 * 
 * @param pfnHandler  Pointer to the callback function.
 * 
 * @remarks Events have the following function definition:
 *  LORAWAN_EVENT_MAC_PROCESS
 *      void callback (void)
 * 
 *  LORAWAN_EVENT_NVM_DATA_CHANGE
 *      void callback (LmHandlerNvmContextStates_t, uint16_t)
 * 
 *  LORAWAN_EVENT_NETWORK_PARAMETERS_CHANGE
 *      void callback (CommissioningParams_t *)
 * 
 *  LORAWAN_EVENT_MAC_MCPS_REQUEST
 *      void callback (LoRaMacStatus_t, McpsReq_t *, TimerTime_t)
 * 
 *  LORAWAN_EVENT_MAC_MLME_REQUEST
 *      void callback (LoRaMacStatus_t, MlmeReq_t *, TimerTime_t)
 * 
 *  LORAWAN_EVENT_JOIN_REQUEST
 *      void callback (LmHandlerJoinParams_t *)
 * 
 *  LORAWAN_EVENT_TX_DATA
 *      void callback (LmHandlerTxParams_t *)
 * 
 *  LORAWAN_EVENT_RX_DATA
 *      void callback (LmHandlerAppData_t *, LmHandlerRxParams_t *)
 * 
 *  LORAWAN_EVENT_CLASS_CHANGE
 *      void callback (DeviceClass_t)
 * 
 *  LORAWAN_EVENT_BEACON_STATUS_CHANGE
 *      void callback (LoRaMacHandlerBeaconParams_t *)
 * 
 *  LORAWAN_EVENT_SYS_TIME_UPDATE
 *      void callback (bool, int32_t)
 * 
 *  LORAWAN_EVENT_SLEEP
 *      void callback (void)
 * 
 *  LORAWAN_EVENT_WAKE
 *      void callback (void)
 */
extern void lorawan_event_callback_register(lorawan_event_e eEvent,
                                            lorawan_event_callback_t pfnHandler);

/**
 * @brief Unregister callback for a LoRaWAN event.
 * 
 * @param eEvent 
 */
extern void lorawan_event_callback_unregister(lorawan_event_e eEvent);

/**
 * @brief Enable or disable debug messages printing.
 * 
 * @param ui32Enabled 
 */
extern void lorawan_tracing_set(uint32_t ui32Enabled);

#ifdef __cplusplus
}
#endif

#endif
