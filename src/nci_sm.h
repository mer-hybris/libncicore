/*
 * Copyright (C) 2019-2021 Jolla Ltd.
 * Copyright (C) 2019-2021 Slava Monich <slava.monich@jolla.com>
 *
 * You may use this file under the terms of BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the names of the copyright holders nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NCI_STATE_MACHINE_H
#define NCI_STATE_MACHINE_H

#include "nci_types_p.h"

/* RF Communication State Machine */

typedef enum nci_interface_version {
    NCI_INTERFACE_VERSION_UNKNOWN,
    NCI_INTERFACE_VERSION_1,
    NCI_INTERFACE_VERSION_2
} NCI_INTERFACE_VERSION;

typedef enum nci_options {
    NCI_OPTION_NONE                     = 0x00,
    NCI_OPTION_POLL_F                   = 0x01,
    NCI_OPTION_LISTEN_F                 = 0x02,
    NCI_OPTION_POLL_V                   = 0x04,
    NCI_OPTION_LISTEN_V                 = 0x08
} NCI_OPTIONS;

#define NCI_OPTION_TYPE_F   (NCI_OPTION_POLL_F|NCI_OPTION_LISTEN_F)
#define NCI_OPTION_TYPE_V   (NCI_OPTION_POLL_V|NCI_OPTION_LISTEN_V)
#define NCI_OPTIONS_DEFAULT NCI_OPTION_TYPE_F /* A and B support always on*/

/* Table 9: NFCC Features */
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

/* Table 31: LA_SEL_INFO coding */
typedef enum nci_la_sel_info {
    NCI_LA_SEL_INFO_ISO_DEP            = 0x20,
    NCI_LA_SEL_INFO_NFC_DEP            = 0x40
} NCI_LA_SEL_INFO;

/* Table 36: Supported Protocols for Listen F */
typedef enum nci_lf_protocol_type {
    NCI_LF_PROTOCOL_TYPE_NFC_DEP       = 0x02
} NCI_LF_PROTOCOL_TYPE;

/* Table 43: Value Field for Mode */
#define NCI_DISCOVER_MAP_MODE_POLL   (0x01)
#define NCI_DISCOVER_MAP_MODE_LISTEN (0x02)

/* Table 46: TLV Coding for Listen Mode Routing */
#define NCI_ROUTING_ENTRY_TYPE_TECHNOLOGY (0x00)
#define NCI_ROUTING_ENTRY_TYPE_PROTOCOL (0x01)
#define NCI_ROUTING_ENTRY_TYPE_AID (0x02)

/* Table 50: Value Field for Power State */
#define NCI_ROUTING_ENTRY_POWER_ON (0x01)
#define NCI_ROUTING_ENTRY_POWER_OFF (0x02)
#define NCI_ROUTING_ENTRY_POWER_BATTERY_OFF (0x04)
#define NCI_ROUTING_ENTRY_POWER_ALL (0x07)

/* Table 63: Deactivation Types */
typedef enum nci_deactivation_type {
    NCI_DEACTIVATE_TYPE_IDLE = 0x00,
    NCI_DEACTIVATE_TYPE_SLEEP = 0x01,
    NCI_DEACTIVATE_TYPE_SLEEP_AF = 0x02,
    NCI_DEACTIVATE_TYPE_DISCOVERY = 0x03
} NCI_DEACTIVATION_TYPE;

/* Table 64: Deactivation Reasons */
typedef enum nci_deactivation_reason {
    NCI_DEACTIVATION_REASON_DH_REQUEST = 0x00,
    NCI_DEACTIVATION_REASON_ENDPOINT_REQUEST = 0x01,
    NCI_DEACTIVATION_REASON_RF_LINK_LOSS = 0x02,
    NCI_DEACTIVATION_REASON_BAD_AFI = 0x03
} NCI_DEACTIVATION_REASON;

/* Table 84: NFCEE IDs */
#define NCI_NFCEE_ID_DH (0x00)

/* Table 94: Status Codes */
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

/* Table 95: RF Technologies */
typedef enum nci_rf_technology {
    NCI_RF_TECHNOLOGY_A = 0x00,
    NCI_RF_TECHNOLOGY_B = 0x01,
    NCI_RF_TECHNOLOGY_F = 0x02,
    NCI_RF_TECHNOLOGY_V = 0x03
} NCI_RF_TECHNOLOGY;

/* Table 101: Configuration Parameter Tags */

/* Common Discovery Parameters */
#define NCI_CONFIG_TOTAL_DURATION               (0x00)
#define NCI_CONFIG_CON_DEVICES_LIMIT            (0x01)

/* Poll Mode: NFC-A Discovery Parameters */
#define NCI_CONFIG_PA_BAIL_OUT                  (0x08)

/* Poll Mode: NFC-B Discovery Parameters */
#define NCI_CONFIG_PB_AFI                       (0x10)
#define NCI_CONFIG_PB_BAIL_OUT                  (0x11)
#define NCI_CONFIG_PB_ATTRIB_PARAM1             (0x12)
#define NCI_CONFIG_PB_SENSB_REQ_PARAM           (0x13)

/* Poll Mode: NFC-F Discovery Parameters */
#define NCI_CONFIG_PF_BIT_RATE                  (0x18)
#define NCI_CONFIG_PF_RC_CODE                   (0x19)

/* Poll Mode: ISO-DEP Discovery Parameters */
#define NCI_CONFIG_PB_H_INFO                    (0x20)
#define NCI_CONFIG_PI_BIT_RATE                  (0x21)
#define NCI_CONFIG_PA_ADV_FEAT                  (0x22)

/* Poll Mode: NFC-DEP Discovery Parameters */
#define NCI_CONFIG_PN_NFC_DEP_SPEED             (0x28)
#define NCI_CONFIG_PN_ATR_REQ_GEN_BYTES         (0x29)
#define NCI_CONFIG_PN_ATR_REQ_CONFIG            (0x2A)

/* Listen Mode: NFC-A Discovery Parameters */
#define NCI_CONFIG_LA_BIT_FRAME_SDD             (0x30)
#define NCI_CONFIG_LA_PLATFORM_CONFIG           (0x31)
#define NCI_CONFIG_LA_SEL_INFO                  (0x32)
#define NCI_CONFIG_LA_NFCID1                    (0x33)

/* Listen Mode: NFC-B Discovery Parameters */
#define NCI_CONFIG_LB_SENSB_INFO                (0x38)
#define NCI_CONFIG_LB_NFCID0                    (0x39)
#define NCI_CONFIG_LB_APPLICATION_DATA          (0x3A)
#define NCI_CONFIG_LB_SFGI                      (0x3B)
#define NCI_CONFIG_LB_ADC_FO                    (0x3C)

/* Listen Mode: NFC-F Discovery Parameters */
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
#define NCI_CONFIG_LF_PROTOCOL_TYPE             (0x50)
#define NCI_CONFIG_LF_T3T_PMM                   (0x51)
#define NCI_CONFIG_LF_T3T_MAX                   (0x52)
#define NCI_CONFIG_LF_T3T_FLAGS                 (0x53)
#define NCI_CONFIG_LF_CON_BITR_F                (0x54)
#define NCI_CONFIG_LF_ADV_FEAT                  (0x55)

/* Listen Mode: ISO-DEP Discovery Parameters */
#define NCI_CONFIG_LI_FWI                       (0x58)
#define NCI_CONFIG_LA_HIST_BY                   (0x59)
#define NCI_CONFIG_LB_H_INFO_RESP               (0x5A)
#define NCI_CONFIG_LI_BIT_RATE                  (0x5B)

/* Listen Mode: NFC-DEP Discovery Parameters */
#define NCI_CONFIG_LN_WT                        (0x60)
#define NCI_CONFIG_LN_ATR_RES_GEN_BYTES         (0x61)
#define NCI_CONFIG_LN_ATR_RES_CONFIG            (0x62)
#define NCI_CONFIG_RF_FIELD_INFO                (0x80)
#define NCI_CONFIG_RF_NFCEE_ACTION              (0x81)
#define NCI_CONFIG_NFCDEP_OP                    (0x82)

/* Table 102: GID and OID Definitions */

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

typedef struct nci_sm_io NciSmIo;

struct nci_sm_io {
    NciSar* sar;
    gboolean (*send)(NciSmIo* io, guint8 gid, guint8 oid,
        GBytes* payload, NciSmResponseFunc resp, gpointer user_data);
    void (*cancel)(NciSmIo* io);
};

struct nci_sm {
    NciSmIo* io;
    NciState* last_state;
    NciState* next_state;
    GBytes* rf_interfaces;
    guint max_routing_table_size;
    NCI_OPTIONS options;
    NCI_INTERFACE_VERSION version;
    NCI_NFCC_DISCOVERY nfcc_discovery;
    NCI_NFCC_ROUTING nfcc_routing;
    NCI_NFCC_POWER nfcc_power;
    NCI_OP_MODE op_mode;
    guint8 llc_version;
    guint16 llc_wks;
};

typedef
void
(*NciSmFunc)(
    NciSm* sm,
    void* user_data);

typedef
void
(*NciSmIntfActivationFunc)(
    NciSm* sm,
    const NciIntfActivationNtf* ntf,
    void* user_data);

/* Universal interface, used all over the place */

NciState*
nci_sm_enter_state(
    NciSm* sm,
    NCI_STATE state,
    NciParam* param)
    NCI_INTERNAL;

void
nci_sm_switch_to(
    NciSm* sm,
    NCI_STATE id)
    NCI_INTERNAL;

void
nci_sm_stall(
    NciSm* sm,
    NCI_STALL type)
    NCI_INTERNAL;

/* Interface for NciCore */

NciSm*
nci_sm_new(
    NciSmIo* io)
    NCI_INTERNAL;

void
nci_sm_free(
    NciSm* sm)
    NCI_INTERNAL;

void
nci_sm_set_op_mode(
    NciSm* sm,
    NCI_OP_MODE op_mode)
    NCI_INTERNAL;

void
nci_sm_handle_ntf(
    NciSm* sm,
    guint8 gid,
    guint8 oid,
    const GUtilData* payload)
    NCI_INTERNAL;

void
nci_sm_add_state(
    NciSm* sm,
    NciState* state)
    NCI_INTERNAL;

void
nci_sm_add_transition(
    NciSm* sm,
    NCI_STATE state,
    NciTransition* transition)
    NCI_INTERNAL;

gulong
nci_sm_add_last_state_handler(
    NciSm* sm,
    NciSmFunc func,
    void* user_data)
    NCI_INTERNAL;

gulong
nci_sm_add_next_state_handler(
    NciSm* sm,
    NciSmFunc func,
    void* user_data)
    NCI_INTERNAL;

gulong
nci_sm_add_intf_activated_handler(
    NciSm* sm,
    NciSmIntfActivationFunc func,
    void* user_data)
    NCI_INTERNAL;

void
nci_sm_remove_handler(
    NciSm* sm,
    gulong id)
    NCI_INTERNAL;

void
nci_sm_remove_handlers(
    NciSm* sm,
    gulong* ids,
    guint count)
    NCI_INTERNAL;

#define nci_sm_remove_all_handlers(sm,ids) \
    nci_sm_remove_handlers(sm, ids, G_N_ELEMENTS(ids))

/* Interface for states and transitions */

void
nci_sm_add_weak_pointer(
    NciSm** ptr)
    NCI_INTERNAL;

void
nci_sm_remove_weak_pointer(
    NciSm** ptr)
    NCI_INTERNAL;

NciState*
nci_sm_get_state(
    NciSm* sm,
    NCI_STATE state)
    NCI_INTERNAL;

NciSar*
nci_sm_sar(
    NciSm* sm)
    NCI_INTERNAL;

gboolean
nci_sm_supports_protocol(
    NciSm* sm,
    NCI_PROTOCOL protocol)
    NCI_INTERNAL;

gboolean
nci_sm_active_transition(
    NciSm* sm,
    NciTransition* transition)
    NCI_INTERNAL;

gboolean
nci_sm_send_command(
    NciSm* sm,
    guint8 gid,
    guint8 oid,
    GBytes* payload,
    NciSmResponseFunc resp,
    gpointer user_data)
    NCI_INTERNAL;

gboolean
nci_sm_send_command_static(
    NciSm* sm,
    guint8 gid,
    guint8 oid,
    const void* payload,
    gsize payload_len,
    NciSmResponseFunc resp,
    gpointer user_data)
    NCI_INTERNAL;

void
nci_sm_intf_activated(
    NciSm* sm,
    const NciIntfActivationNtf* ntf)
    NCI_INTERNAL;

void
nci_sm_handle_conn_credits_ntf(
    NciSm* sm,
    const GUtilData* payload)
    NCI_INTERNAL;

void
nci_sm_handle_rf_deactivate_ntf(
    NciSm* sm,
    const GUtilData* payload)
    NCI_INTERNAL;

/* And this one is currently only for unit tests */

extern const char* nci_sm_config_file NCI_INTERNAL;

#endif /* NCI_STATE_MACHINE_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
