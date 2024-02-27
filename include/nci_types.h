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

#ifndef NCI_TYPES_H
#define NCI_TYPES_H

#include <gutil_types.h>

G_BEGIN_DECLS

/* Types */

typedef struct nci_core NciCore;
typedef struct nci_hal_client NciHalClient;
typedef struct nci_hal_io NciHalIo;

/* Conn ID
 * 0: Static RF Connection between the DH and a Remote NFC Endpoint */
#define NCI_STATIC_RF_CONN_ID (0x00)

/* Status Codes */
typedef enum nci_status {
    NCI_STATUS_OK = 0x00,
    NCI_STATUS_REJECTED = 0x01,
    NCI_STATUS_RF_FRAME_CORRUPTED = 0x02,
    NCI_STATUS_FAILED = 0x03,
    NCI_STATUS_NOT_INITIALIZED = 0x04,
    NCI_STATUS_SYNTAX_ERROR = 0x05,
    NCI_STATUS_SEMANTIC_ERROR = 0x06,
    NCI_STATUS_INVALID_PARAM = 0x09,
    NCI_STATUS_MESSAGE_SIZE_EXCEEDED = 0x0A,
    NCI_STATUS_STATUS_OK_1_BIT = 0x11, /* Since 1.1.27 */
    NCI_STATUS_STATUS_OK_2_BIT = 0x12, /* Since 1.1.27 */
    NCI_STATUS_STATUS_OK_3_BIT = 0x13, /* Since 1.1.27 */
    NCI_STATUS_STATUS_OK_4_BIT = 0x14, /* Since 1.1.27 */
    NCI_STATUS_STATUS_OK_5_BIT = 0x15, /* Since 1.1.27 */
    NCI_STATUS_STATUS_OK_6_BIT = 0x16, /* Since 1.1.27 */
    NCI_STATUS_STATUS_OK_7_BIT = 0x17, /* Since 1.1.27 */
    NCI_STATUS_DISCOVERY_ALREADY_STARTED = 0xA0,
    NCI_STATUS_DISCOVERY_TARGET_ACTIVATION_FAILED = 0xA1,
    NCI_STATUS_DISCOVERY_TEAR_DOWN = 0xA2,
    NCI_STATUS_RF_TRANSMISSION_ERROR = 0xB0,
    NCI_STATUS_RF_PROTOCOL_ERROR = 0xB1,
    NCI_STATUS_RF_TIMEOUT_ERROR = 0xB2,
    NCI_STATUS_RF_UNEXPECTED_DATA = 0xB3, /* Since 1.1.27 */
    NCI_STATUS_NFCEE_INTERFACE_ACTIVATION_FAILED = 0xC0,
    NCI_STATUS_NFCEE_TRANSMISSION_ERROR = 0xC1,
    NCI_STATUS_NFCEE_PROTOCOL_ERROR = 0xC2,
    NCI_STATUS_NFCEE_TIMEOUT_ERROR = 0xC3
} NCI_STATUS;

/* RF Technology and Mode */
typedef enum nci_mode {
    NCI_MODE_PASSIVE_POLL_A = 0x00,
    NCI_MODE_PASSIVE_POLL_B = 0x01,
    NCI_MODE_PASSIVE_POLL_F = 0x02,
    NCI_MODE_ACTIVE_POLL_A = 0x03,
    NCI_MODE_ACTIVE_POLL_F = 0x05,
    NCI_MODE_PASSIVE_POLL_V = 0x06, /* Since 1.1.15 */
    NCI_MODE_PASSIVE_LISTEN_A = 0x80,
    NCI_MODE_PASSIVE_LISTEN_B = 0x81,
    NCI_MODE_PASSIVE_LISTEN_F = 0x82,
    NCI_MODE_ACTIVE_LISTEN_A = 0x83,
    NCI_MODE_ACTIVE_LISTEN_F = 0x85,
    NCI_MODE_PASSIVE_LISTEN_V = 0x86 /* Since 1.1.15 */
} NCI_MODE;

/* These names were used before 1.1.15 */
#define NCI_MODE_PASSIVE_POLL_15693   NCI_MODE_PASSIVE_POLL_V
#define NCI_MODE_PASSIVE_LISTEN_15693 NCI_MODE_PASSIVE_LISTEN_V

/* Bit Rates */
typedef enum nci_bit_rate {
    NFC_BIT_RATE_106 = 0x00, /* 106 Kbit/s */
    NFC_BIT_RATE_212 = 0x01, /* 212 Kbit/s */
    NFC_BIT_RATE_424 = 0x02, /* 424 Kbit/s */
    NFC_BIT_RATE_848 = 0x03, /* 848 Kbit/s */
    NFC_BIT_RATE_1695 = 0x04, /* 1695 Kbit/s */
    NFC_BIT_RATE_3390 = 0x05, /* 3390 Kbit/s */
    NFC_BIT_RATE_6780 = 0x06  /* 6780 Kbit/s */
} NFC_BIT_RATE;

/* RF Protocols */
typedef enum nci_protocol {
    NCI_PROTOCOL_UNDETERMINED = 0x00,
    NCI_PROTOCOL_T1T = 0x01,
    NCI_PROTOCOL_T2T = 0x02,
    NCI_PROTOCOL_T3T = 0x03,
    NCI_PROTOCOL_ISO_DEP = 0x04,
    NCI_PROTOCOL_NFC_DEP = 0x05,
    NCI_PROTOCOL_T5T = 0x06,        /* Since 1.1.21 */
    NCI_PROTOCOL_PROPRIETARY = 0x80 /* Since 1.0.6 */
} NCI_PROTOCOL;

/* RF Interfaces */
typedef enum nci_rf_interface {
    NCI_RF_INTERFACE_NFCEE_DIRECT = 0x00,
    NCI_RF_INTERFACE_FRAME = 0x01,
    NCI_RF_INTERFACE_ISO_DEP = 0x02,
    NCI_RF_INTERFACE_NFC_DEP = 0x03,
    NCI_RF_INTERFACE_PROPRIETARY = 0x80 /* Since 1.0.6 */
} NCI_RF_INTERFACE;

/* Specific Parameters for NFC-A Poll Mode */
typedef struct nci_mode_param_poll_a {
    guint8 sens_res[2];
    guint8 nfcid1_len;
    guint8 nfcid1[10];
    guint8 sel_res_len;
    guint8 sel_res;
} NciModeParamPollA;

/* Specific Parameters for NFC-B Poll Mode */
typedef struct nci_mode_param_poll_b {
    guint8 nfcid0[4];
    guint fsc;  /* FSCI converted to bytes */
    /* Since 1.1.5 */
    guint8 app_data[4];
    GUtilData prot_info;
} NciModeParamPollB;

/* Specific Parameters for NFC-F Poll Mode */
typedef struct nci_mode_param_poll_f {
    guint8 bitrate;   /* 1 = 212 kbps, 2 = 424 kbps */
    guint8 nfcid2[8]; /* Bytes 2-9 of SENSF_RES */
} NciModeParamPollF;  /* Since 1.0.8 */

/* Specific Parameters for NFC-V Poll Mode */
typedef struct nci_mode_param_poll_v {
    guint8 res_flag;  /* 1st Byte of the INVENTORY_RES Response */
    guint8 dsfid;     /* 2nd Byte of the INVENTORY_RES Response */
    guint8 uid[8];    /* 3rd Byte to last Byte of the INVENTORY_RES */
} NciModeParamPollV;  /* Since 1.1.21 */

/* Specific Parameters for NFC-F Listen Mode */
typedef struct nci_mode_param_listen_f {
    GUtilData nfcid2; /* NFCID2 generated by the Local NFCC */
} NciModeParamListenF; /* Since 1.1.2 */

typedef union nci_mode_param {
    NciModeParamPollA poll_a;
    NciModeParamPollB poll_b;
    NciModeParamPollF poll_f;     /* Since 1.0.8 */
    NciModeParamPollV poll_v;     /* Since 1.1.21 */
    NciModeParamListenF listen_f; /* Since 1.1.2 */
} NciModeParam;

/* Activation Parameters for NFC-A/ISO-DEP Poll Mode */
typedef struct nci_activation_param_iso_dep_poll_a {
    guint fsc;     /* FSC (FSCI converted to bytes) */
    GUtilData t1;  /* T1 to Tk (otherwise called historical bytes) */
    /* Since 1.1.11 */
    guint8 t0;     /* Format byte T0 */

#define NFC_T4A_ATS_T0_A         (0x10) /* TA is transmitted */
#define NFC_T4A_ATS_T0_B         (0x20) /* TB is transmitted */
#define NFC_T4A_ATS_T0_C         (0x40) /* TC is transmitted */
#define NFC_T4A_ATS_T0_FSCI_MASK (0x0f) /* FSCI mask */

    guint8 ta;     /* Interface byte TA (optional) */
    guint8 tb;     /* Interface byte TB (optional) */
    guint8 tc;     /* Interface byte TC (optional) */
} NciActivationParamIsoDepPollA;

/* Activation Parameters for NFC-B/ISO-DEP Poll Mode */
typedef struct nci_activation_param_iso_dep_poll_b {
    guint mbli;     /* Maximum Buffer Length Index */
    guint did;      /* Device ID */
    GUtilData hlr;  /* Higher Layer Response */
} NciActivationParamIsoDepPollB; /* Since 1.1.5 */

/* Activation Parameters for NFC-A/ISO-DEP Listen Mode */
typedef struct nci_activation_param_iso_dep_listen_a {
    guint fsd;      /* Frame Size (bytes) */
    guint did;      /* Device ID */
} NciActivationParamIsoDepListenA; /* Since 1.1.21 */

/* Activation Parameters for NFC-B/ISO-DEP Listen Mode */
typedef struct nci_activation_param_iso_dep_listen_b {
    guint8 nfcid0[4];
    guint8 param[4];  /* Params 1-4 */
    GUtilData hlc;    /* Higher Layer Command */
} NciActivationParamIsoDepListenB; /* Since 1.1.21 */

/* Activation Parameters for NFC-DEP Poll Mode */
typedef struct nci_activation_param_nfc_dep_poll {
    /* ATR_RES starting from and including Byte 3 */
    guint8 nfcid3[10];
    guint8 did;
    guint8 bs;
    guint8 br;
    guint8 to;
    guint8 pp;
    GUtilData g;
} NciActivationParamNfcDepPoll; /* Since 1.0.8 */

/* Activation Parameters for NFC-DEP Listen Mode */
typedef struct nci_activation_param_nfc_dep_listen {
    /* ATR_REQ starting from and including Byte 3 */
    guint8 nfcid3[10];
    guint8 did;
    guint8 bs;
    guint8 br;
    guint8 pp;
    GUtilData g;
} NciActivationParamNfcDepListen; /* Since 1.0.8 */

typedef union nci_activation_param {
    NciActivationParamIsoDepPollA iso_dep_poll_a;
    NciActivationParamIsoDepPollB iso_dep_poll_b;      /* Since 1.1.5 */
    NciActivationParamIsoDepListenA iso_dep_listen_a;  /* Since 1.1.21 */
    NciActivationParamIsoDepListenB iso_dep_listen_b;  /* Since 1.1.21 */
    NciActivationParamNfcDepPoll nfc_dep_poll;         /* Since 1.0.8 */
    NciActivationParamNfcDepListen nfc_dep_listen;     /* Since 1.0.8 */
} NciActivationParam;

/* Notification for RF Interface activation */
typedef struct nci_intf_activated_ntf {
    guint8 discovery_id;
    NCI_RF_INTERFACE rf_intf;
    NCI_PROTOCOL protocol;
    NCI_MODE mode;
    guint8 max_data_packet_size;
    guint8 num_credits;
    guint8 mode_param_len;
    const void* mode_param_bytes;
    const NciModeParam* mode_param;
    NCI_MODE data_exchange_mode;
    NFC_BIT_RATE transmit_rate;
    NFC_BIT_RATE receive_rate;
    guint8 activation_param_len;
    const void* activation_param_bytes;
    const NciActivationParam* activation_param;
} NciIntfActivationNtf;

/* Control Messages to Start Discovery */
typedef struct nci_discovery_ntf {
    guint8 discovery_id;
    NCI_PROTOCOL protocol;
    NCI_MODE mode;
    guint8 param_len;
    const void* param_bytes;
    const NciModeParam* param;
    gboolean last;
} NciDiscoveryNtf;

/* NFCID1 can be 4, 7, or 10 bytes long. */
typedef struct nci_nfcid {
    guint8 len;
    guint8 bytes[10];
} NciNfcid1; /* Since 1.1.22 */

/* This is essentially NCI_MODE as a bitmask */

typedef enum nci_tech {
    NCI_TECH_NONE             = 0x0000,
    NCI_TECH_A_POLL_PASSIVE   = 0x0001,
    NCI_TECH_A_POLL_ACTIVE    = 0x0002,
    NCI_TECH_A_LISTEN_PASSIVE = 0x0004,
    NCI_TECH_A_LISTEN_ACTIVE  = 0x0008,
    NCI_TECH_B_POLL           = 0x0010,
    NCI_TECH_B_LISTEN         = 0x0040,
    NCI_TECH_F_POLL_PASSIVE   = 0x0100,
    NCI_TECH_F_POLL_ACTIVE    = 0x0200,
    NCI_TECH_F_LISTEN_PASSIVE = 0x0400,
    NCI_TECH_F_LISTEN_ACTIVE  = 0x0800,
    NCI_TECH_V_POLL           = 0x1000,
    NCI_TECH_V_LISTEN         = 0x4000
} NCI_TECH; /* Since 1.1.21 */

#define NCI_TECH_A_POLL   (NCI_TECH_A_POLL_PASSIVE|NCI_TECH_A_POLL_ACTIVE)
#define NCI_TECH_A_LISTEN (NCI_TECH_A_LISTEN_PASSIVE|NCI_TECH_A_LISTEN_ACTIVE)
#define NCI_TECH_A        (NCI_TECH_A_POLL|NCI_TECH_A_LISTEN)
#define NCI_TECH_B        (NCI_TECH_B_POLL|NCI_TECH_B_LISTEN)
#define NCI_TECH_F_POLL   (NCI_TECH_F_POLL_PASSIVE|NCI_TECH_F_POLL_ACTIVE)
#define NCI_TECH_F_LISTEN (NCI_TECH_F_LISTEN_PASSIVE|NCI_TECH_F_LISTEN_ACTIVE)
#define NCI_TECH_F        (NCI_TECH_F_POLL|NCI_TECH_F_LISTEN)
#define NCI_TECH_V        (NCI_TECH_V_POLL|NCI_TECH_V_LISTEN)
#define NCI_TECH_ALL      (NCI_TECH_A|NCI_TECH_B|NCI_TECH_F|NCI_TECH_V)

/* NCI states */

typedef enum nci_state_id {
    NCI_STATE_INIT,
    NCI_STATE_ERROR,
    NCI_STATE_STOP,
    /* RFST states are taken from the NCI spec */
    NCI_RFST_IDLE,
    NCI_RFST_DISCOVERY,
    NCI_RFST_W4_ALL_DISCOVERIES,
    NCI_RFST_W4_HOST_SELECT,
    NCI_RFST_POLL_ACTIVE,
    NCI_RFST_LISTEN_ACTIVE,
    NCI_RFST_LISTEN_SLEEP,
    NCI_CORE_STATES
} NCI_STATE;

/*
 * Operation modes
 *
 * The relationship between op mode bits goes like this:
 *
 * +----------------+---------------------+----------------+
 * | NFC R/W Modes  |   NFC Peer Modes    | NFC CE Mode    |
 * | NFC_OP_MODE_RW |   NFC_OP_MODE_PEER  | NFC_OP_MODE_CE |
 * +------+---------+-----------+---------+----------------+
 * | Tags | ISO-DEP | NFC-DEP   | NFC-DEP | ISO-DEP        |
 * | 1-3  |         | Initiator | Target  |                |
 * +------+---------+-----------+---------+----------------+
 * |      Poll side             |     Listen side          |
 * |      NFC_OP_MODE_POLL      |     NFC_OP_MODE_LISTEN   |
 * +----------------------------+--------------------------+
 *
 * That hopefully explains why certain combinations don't make
 * sense, specifically (NFC_OP_MODE_RW | NFC_OP_MODE_LISTEN) and
 * (NFC_OP_MODE_CE | NFC_OP_MODE_POLL).
 *
 * Note that NFC_OP_MODE_RW enables all appropriate poll modes
 * even without NFC_OP_MODE_POLL and NFC_OP_MODE_CE enables the
 * listen modes even without NFC_OP_MODE_LISTEN.
 *
 * NFC_OP_MODE_PEER, however, doesn't have any effect if neither
 * NFC_OP_MODE_POLL nor NFC_OP_MODE_LISTEN is set.
 */

typedef enum nci_op_mode {
    NFC_OP_MODE_NONE = 0x00,
    NFC_OP_MODE_RW = 0x01,      /* Reader/Writer (requires POLL) */
    NFC_OP_MODE_PEER = 0x02,    /* Peer NFC-DEP (POLL and/or LISTEN) */
    NFC_OP_MODE_CE = 0x04,      /* Card Emulation (requires LISTEN) */
    /* The next two are kind of orthogonal, see the diagram above */
    NFC_OP_MODE_POLL = 0x08,    /* Poll side/Initiator */
    NFC_OP_MODE_LISTEN = 0x10   /* Listen side/Target */
} NCI_OP_MODE; /* Since 1.1.0 */

/* Logging */

#define NCI_LOG_MODULE nci_log
extern GLogModule NCI_LOG_MODULE;

G_END_DECLS

#endif /* NFC_TYPES_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
