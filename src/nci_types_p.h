/*
 * Copyright (C) 2018-2024 Slava Monich <slava@monich.com>
 * Copyright (C) 2018-2020 Jolla Ltd.
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer
 *     in the documentation and/or other materials provided with the
 *     distribution.
 *
 *  3. Neither the names of the copyright holders nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * any official policies, either expressed or implied.
 */

#ifndef NCI_TYPES_PRIVATE_H
#define NCI_TYPES_PRIVATE_H

#include <nci_types.h>

#define NCI_INTERNAL G_GNUC_INTERNAL

typedef struct nci_param NciParam;
typedef struct nci_sar NciSar;
typedef struct nci_sm NciSm;
typedef struct nci_state NciState;
typedef struct nci_transition NciTransition;

typedef enum nci_request_status {
    NCI_REQUEST_SUCCESS,
    NCI_REQUEST_TIMEOUT,
    NCI_REQUEST_CANCELLED
} NCI_REQUEST_STATUS;

typedef
void
(*NciSmResponseFunc)(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    gpointer user_data);

/* Stall modes */
typedef enum nci_stall {
    NCI_STALL_STOP,
    NCI_STALL_ERROR
} NCI_STALL;

/* Message Type (MT) */
#define NCI_MT_MASK     (0xe0)
#define NCI_MT_DATA_PKT (0x00)
#define NCI_MT_CMD_PKT  (0x20)
#define NCI_MT_RSP_PKT  (0x40)
#define NCI_MT_NTF_PKT  (0x60)

/* Packet Boundary Flag (PBF) */
#define NCI_PBF         (0x10)

/* NCI protocol version */
typedef enum nci_interface_version {
    NCI_INTERFACE_VERSION_UNKNOWN,
    NCI_INTERFACE_VERSION_1,
    NCI_INTERFACE_VERSION_2
} NCI_INTERFACE_VERSION;

/* NFCC Features */
typedef enum nci_nfcc_discovery {
    NCI_NFCC_DISCOVERY_NONE             = 0x00,
    NCI_NFCC_DISCOVERY_FREQUENCY_CONFIG = 0x01,
    NCI_NFCC_DISCOVERY_RF_CONFIG_MERGE  = 0x02
} NCI_NFCC_DISCOVERY;

typedef enum nci_nfcc_routing {
    NCI_NFCC_ROUTING_NONE               = 0x00,
    NCI_NFCC_ROUTING_TECHNOLOGY_BASED   = 0x02,
    NCI_NFCC_ROUTING_PROTOCOL_BASED     = 0x04,
    NCI_NFCC_ROUTING_AID_BASED          = 0x08
} NCI_NFCC_ROUTING;

typedef enum nci_nfcc_power {
    NCI_NFCC_POWER_NONE                 = 0x00,
    NCI_NFCC_POWER_BATTERY_OFF          = 0x01,
    NCI_NFCC_POWER_SWITCH_OFF           = 0x02
} NCI_NFCC_POWER;

/* LA_SENS_RES_1 (Byte 1 of SENS_RES) coding */
typedef enum nci_la_sens_res_1 {
    NCI_LA_SENS_RES_NFCID1_LEN_4        = 0x00,
    NCI_LA_SENS_RES_NFCID1_LEN_7        = 0x40,
    NCI_LA_SENS_RES_NFCID1_LEN_10       = 0x80,
    NCI_LA_SENS_RES_NFCID1_LEN_MASK     = 0xc0
} NCI_LA_SENS_RES_1;

/* LA_SEL_INFO coding */
typedef enum nci_la_sel_info {
    NCI_LA_SEL_INFO_ISO_DEP            = 0x20,
    NCI_LA_SEL_INFO_NFC_DEP            = 0x40
} NCI_LA_SEL_INFO;

/* Supported Protocols for Listen F */
typedef enum nci_lf_protocol_type {
    NCI_LF_PROTOCOL_TYPE_NFC_DEP       = 0x02
} NCI_LF_PROTOCOL_TYPE;

/* Value Field for Mode */
#define NCI_DISCOVER_MAP_MODE_POLL   (0x01)
#define NCI_DISCOVER_MAP_MODE_LISTEN (0x02)

/* TLV Coding for Listen Mode Routing */
#define NCI_ROUTING_ENTRY_TYPE_TECHNOLOGY (0x00)
#define NCI_ROUTING_ENTRY_TYPE_PROTOCOL (0x01)
#define NCI_ROUTING_ENTRY_TYPE_AID (0x02)

/* Value Field for Power State */
#define NCI_ROUTING_ENTRY_POWER_ON (0x01)
#define NCI_ROUTING_ENTRY_POWER_OFF (0x02)
#define NCI_ROUTING_ENTRY_POWER_BATTERY_OFF (0x04)
#define NCI_ROUTING_ENTRY_POWER_ALL (0x07)

/* Deactivation Types */
typedef enum nci_deactivation_type {
    NCI_DEACTIVATE_TYPE_IDLE = 0x00,
    NCI_DEACTIVATE_TYPE_SLEEP = 0x01,
    NCI_DEACTIVATE_TYPE_SLEEP_AF = 0x02,
    NCI_DEACTIVATE_TYPE_DISCOVERY = 0x03
} NCI_DEACTIVATION_TYPE;

/* Deactivation Reasons */
typedef enum nci_deactivation_reason {
    NCI_DEACTIVATION_REASON_DH_REQUEST = 0x00,
    NCI_DEACTIVATION_REASON_ENDPOINT_REQUEST = 0x01,
    NCI_DEACTIVATION_REASON_RF_LINK_LOSS = 0x02,
    NCI_DEACTIVATION_REASON_BAD_AFI = 0x03
} NCI_DEACTIVATION_REASON;

/* NFCEE IDs */
#define NCI_NFCEE_ID_DH (0x00)

/* Status Codes */
#define NCI_STATUS_OK                           (0x00)
#define NCI_STATUS_REJECTED                     (0x01)
#define NCI_STATUS_RF_FRAME_CORRUPTED           (0x02)
#define NCI_STATUS_FAILED                       (0x03)
#define NCI_STATUS_NOT_INITIALIZED              (0x04)
#define NCI_STATUS_SYNTAX_ERROR                 (0x05)
#define NCI_STATUS_SEMANTIC_ERROR               (0x06)
#define NCI_STATUS_INVALID_PARAM                (0x09)
#define NCI_STATUS_MESSAGE_SIZE_EXCEEDED        (0x0a)
#define NCI_DISCOVERY_ALREADY_STARTED           (0xa0)
#define NCI_DISCOVERY_TARGET_ACTIVATION_FAILED  (0xa1)
#define NCI_DISCOVERY_TEAR_DOWN                 (0xa2)
#define NCI_RF_TRANSMISSION_ERROR               (0xb0)
#define NCI_RF_PROTOCOL_ERROR                   (0xb1)
#define NCI_RF_TIMEOUT_ERROR                    (0xb2)
#define NCI_NFCEE_INTERFACE_ACTIVATION_FAILED   (0xc0)
#define NCI_NFCEE_TRANSMISSION_ERROR            (0xc1)
#define NCI_NFCEE_PROTOCOL_ERROR                (0xc2)
#define NCI_NFCEE_TIMEOUT_ERROR                 (0xc3)

/* RF Technologies */
typedef enum nci_rf_technology {
    NCI_RF_TECHNOLOGY_A = 0x00,
    NCI_RF_TECHNOLOGY_B = 0x01,
    NCI_RF_TECHNOLOGY_F = 0x02,
    NCI_RF_TECHNOLOGY_V = 0x03
} NCI_RF_TECHNOLOGY;

/* Configuration Parameter Tags */

/* Common Discovery Parameters */
#define NCI_CONFIG_TOTAL_DURATION               (0x00)
#define NCI_CONFIG_CON_DEVICES_LIMIT            (0x01)
#define NCI_CONFIG_CON_DISCOVERY_PARAM          (0x02) /* NCI 2.0 */
#define NCI_CONFIG_POWER_STATE                  (0x03) /* NCI 2.0 */

/* Poll Mode: NFC-A Discovery Parameters */
#define NCI_CONFIG_PA_BAIL_OUT                  (0x08)
#define NCI_CONFIG_PA_DEVICES_LIMIT             (0x09) /* NCI 2.0 */

/* Poll Mode: NFC-B Discovery Parameters */
#define NCI_CONFIG_PB_AFI                       (0x10)
#define NCI_CONFIG_PB_BAIL_OUT                  (0x11)
#define NCI_CONFIG_PB_ATTRIB_PARAM1             (0x12)
#define NCI_CONFIG_PB_SENSB_REQ_PARAM           (0x13)
#define NCI_CONFIG_PB_DEVICES_LIMIT             (0x14) /* NCI 2.0 */

/* Poll Mode: NFC-F Discovery Parameters */
#define NCI_CONFIG_PF_BIT_RATE                  (0x18)
#define NCI_CONFIG_PF_BAIL_OUT                  (0x19)
#define NCI_CONFIG_PF_DEVICES_LIMIT             (0x1A) /* NCI 2.0 */

/* Poll Mode: ISO-DEP Discovery Parameters */
#define NCI_CONFIG_PB_H_INFO                    (0x20)
#define NCI_CONFIG_PI_BIT_RATE                  (0x21)
#define NCI_CONFIG_PA_ADV_FEAT                  (0x22)

/* Poll Mode: NFC-DEP Discovery Parameters */
#define NCI_CONFIG_PN_NFC_DEP_SPEED             (0x28)
#define NCI_CONFIG_PN_ATR_REQ_GEN_BYTES         (0x29)
#define NCI_CONFIG_PN_ATR_REQ_CONFIG            (0x2A)

/* Poll Mode: NFC-V Discovery Parameters */
#define NCI_CONFIG_PV_DEVICES_LIMIT             (0x2F) /* NCI 2.0 */

/* Listen Mode: NFC-A Discovery Parameters */
#define NCI_CONFIG_LA_SENS_RES_1                (0x30)
#define NCI_CONFIG_LA_SENS_RES_2                (0x31)
#define NCI_CONFIG_LA_SEL_INFO                  (0x32)
#define NCI_CONFIG_LA_NFCID1                    (0x33)

/* Listen Mode: NFC-B Discovery Parameters */
#define NCI_CONFIG_LB_SENSB_INFO                (0x38)
#define NCI_CONFIG_LB_NFCID0                    (0x39)
#define NCI_CONFIG_LB_APPLICATION_DATA          (0x3A)
#define NCI_CONFIG_LB_SFGI                      (0x3B)
#define NCI_CONFIG_LB_ADC_FO                    (0x3C)
#define NCI_CONFIG_LB_BIT_RATE                  (0x3E) /* NCI 2.0 */

/* Listen Mode: NFC-F Discovery Parameters */
#define NCI_CONFIG_LF_PROTOCOL_TYPE             (0x50)

/* Listen Mode: T3T Discovery Parameters */
#define NCI_CONFIG_LF_T3T_IDENTIFIERS_1         (0x40)
#define NCI_CONFIG_LF_T3T_IDENTIFIERS_2         (0x41)
#define NCI_CONFIG_LF_T3T_IDENTIFIERS_3         (0x42)
#define NCI_CONFIG_LF_T3T_IDENTIFIERS_4         (0x43)
#define NCI_CONFIG_LF_T3T_IDENTIFIERS_5         (0x44)
#define NCI_CONFIG_LF_T3T_IDENTIFIERS_6         (0x45)
#define NCI_CONFIG_LF_T3T_IDENTIFIERS_7         (0x46)
#define NCI_CONFIG_LF_T3T_IDENTIFIERS_8         (0x47)
#define NCI_CONFIG_LF_T3T_IDENTIFIERS_9         (0x48)
#define NCI_CONFIG_LF_T3T_IDENTIFIERS_10        (0x49)
#define NCI_CONFIG_LF_T3T_IDENTIFIERS_11        (0x4A)
#define NCI_CONFIG_LF_T3T_IDENTIFIERS_12        (0x4B)
#define NCI_CONFIG_LF_T3T_IDENTIFIERS_13        (0x4C)
#define NCI_CONFIG_LF_T3T_IDENTIFIERS_14        (0x4D)
#define NCI_CONFIG_LF_T3T_IDENTIFIERS_15        (0x4E)
#define NCI_CONFIG_LF_T3T_IDENTIFIERS_16        (0x4F)
#define NCI_CONFIG_LF_T3T_PMM                   (0x51)
#define NCI_CONFIG_LF_T3T_MAX                   (0x52)
#define NCI_CONFIG_LF_T3T_FLAGS                 (0x53)
#define NCI_CONFIG_LF_CON_BITR_F                (0x54)
#define NCI_CONFIG_LF_T3T_RD_ALLOWED            (0x55)

/* Listen Mode: ISO-DEP Discovery Parameters */
#define NCI_CONFIG_LI_FWI                       (0x58)
#define NCI_CONFIG_LA_HIST_BY                   (0x59)
#define NCI_CONFIG_LB_H_INFO_RESP               (0x5A)
#define NCI_CONFIG_LI_BIT_RATE                  (0x5B)
#define NCI_CONFIG_LI_A_RATS_TC1                (0x5C) /* NCI 2.0 */

/* Listen Mode: NFC-DEP Discovery Parameters */
#define NCI_CONFIG_LN_WT                        (0x60)
#define NCI_CONFIG_LN_ATR_RES_GEN_BYTES         (0x61)
#define NCI_CONFIG_LN_ATR_RES_CONFIG            (0x62)

/* Active Poll Mode Parameters */
#define NCI_CONFIG_PACM_BIT_RATE                (0x68) /* NCI 2.0 */

/* Other Parameters */
#define NCI_CONFIG_RF_FIELD_INFO                (0x80)
#define NCI_CONFIG_RF_NFCEE_ACTION              (0x81)
#define NCI_CONFIG_NFCDEP_OP                    (0x82)
#define NCI_CONFIG_LLCP_VERSION                 (0x83) /* NCI 2.0 */
#define NCI_CONFIG_NFCC_CONFIG_CONTROL          (0x85) /* NCI 2.0 */

/* GID and OID Definitions */

/* GID values */
#define NCI_GID_CORE                            (0x00)
#define NCI_GID_RF                              (0x01)
#define NCI_GID_NFCEE                           (0x02)

/* OID values */
#define NCI_OID_CORE_RESET                      (0x00)
#define NCI_OID_CORE_INIT                       (0x01)
#define NCI_OID_CORE_SET_CONFIG                 (0x02)
#define NCI_OID_CORE_GET_CONFIG                 (0x03)
#define NCI_OID_CORE_CONN_CREATE                (0x04)
#define NCI_OID_CORE_CONN_CLOSE                 (0x05)
#define NCI_OID_CORE_CONN_CREDITS               (0x06)
#define NCI_OID_CORE_GENERIC_ERROR              (0x07)
#define NCI_OID_CORE_INTERFACE_ERROR            (0x08)
#define NCI_OID_CORE_SET_POWER_SUB_STATE        (0x09) /* NCI 2.0 */

#define NCI_OID_RF_DISCOVER_MAP                 (0x00)
#define NCI_OID_RF_SET_LISTEN_MODE_ROUTING      (0x01)
#define NCI_OID_RF_GET_LISTEN_MODE_ROUTING      (0x02)
#define NCI_OID_RF_DISCOVER                     (0x03)
#define NCI_OID_RF_DISCOVER_SELECT              (0x04)
#define NCI_OID_RF_INTF_ACTIVATED               (0x05)
#define NCI_OID_RF_DEACTIVATE                   (0x06)
#define NCI_OID_RF_FIELD_INFO                   (0x07)
#define NCI_OID_RF_T3T_POLLING                  (0x08)
#define NCI_OID_RF_NFCEE_ACTION                 (0x09)
#define NCI_OID_RF_NFCEE_DISCOVERY_REQ          (0x0a)
#define NCI_OID_RF_PARAMETER_UPDATE             (0x0b)
#define NCI_OID_RF_INTF_EXT_START               (0x0c) /* NCI 2.0 */
#define NCI_OID_RF_INTF_EXT_STOP                (0x0d) /* NCI 2.0 */
#define NCI_OID_RF_EXT_AGG_ABORT                (0x0e) /* NCI 2.0 */
#define NCI_OID_RF_NDEF_ABORT                   (0x0f) /* NCI 2.0 */
#define NCI_OID_RF_ISO_DEP_NAK_PRESENCE         (0x10) /* NCI 2.0 */
#define NCI_OID_RF_SET_FORCED_NFCEE_ROUTING     (0x11) /* NCI 2.0 */

#endif /* NCI_TYPES_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
