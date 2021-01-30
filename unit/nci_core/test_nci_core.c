/*
 * Copyright (C) 2018-2021 Jolla Ltd.
 * Copyright (C) 2018-2021 Slava Monich <slava.monich@jolla.com>
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

#include "test_common.h"

#include "nci_core.h"
#include "nci_hal.h"
#include "nci_sm.h"

#include <gutil_macros.h>
#include <gutil_misc.h>
#include <gutil_log.h>

#include <glib-object.h> /* For g_type_init() */

static TestOpt test_opt;

#define TEST_DEFAULT_CMD_TIMEOUT (10000) /* milliseconds */
#define TEST_SHORT_TIMEOUT (500) /* milliseconds */

#define TMP_DIR_TEMPLATE "test-nci-core-XXXXXX"
#define CONFIG_SECTION "[Configuration]"
#define CONFIG_ENTRY_TECHNOLOGIES "Technologies"

static const guint8 CORE_RESET_CMD[] = {
    0x20, 0x00, 0x01, 0x00
};
static const guint8 CORE_RESET_RSP[] = {
    0x40, 0x00, 0x03, 0x00, 0x10, 0x00
};
static const guint8 CORE_RESET_RSP_ERROR[] = {
    0x40, 0x00, 0x03, 0x03, 0x10, 0x00
};
static const guint8 CORE_RESET_V2_RSP[] = {
    0x40, 0x00, 0x01, 0x00
};
static const guint8 CORE_RESET_V2_RSP_ERROR[] = {
    0x40, 0x00, 0x01, 0x03
};
static const guint8 CORE_RESET_V2_NTF[] = {
    0x60, 0x00, 0x1f, 0x02, 0x01, 0x20, 0x02, 0x1a,
    0x04, 0x04, 0x01, 0x03, 0x63, 0x94, 0x02, 0x02,
    0x00, 0x59, 0xc0, 0xc0, 0x1b, 0x59, 0xc0, 0x89,
    0x7f, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x42,
    0x22, 0x01
};
static const guint8 CORE_RESET_RSP_BROKEN[] = {
    0x40, 0x00, 0x02, 0x00, 0x00
};
static const guint8 CORE_INIT_CMD_V1[] = {
    0x20, 0x01, 0x00
};
static const guint8 CORE_INIT_CMD_V2[] = {
    0x20, 0x01, 0x02, 0x00, 0x00
};
static const guint8 CORE_INIT_RSP[] = {
    0x40, 0x01, 0x19, 0x00, 0x03, 0x0e, 0x02, 0x00,
    0x08, 0x00, 0x01, 0x02, 0x03, 0x80, 0x82, 0x83,
    0x84, 0x02, 0x5c, 0x03, 0xff, 0x02, 0x00, 0x04,
    0x41, 0x11, 0x01, 0x18
};
static const guint8 CORE_INIT_RSP_1[] = {
    0x40, 0x01, 0x19, 0x00, 0x03, 0x0e, 0x02, 0x00,
    0x08, 0x00, 0x01, 0x02, 0x03, 0x80, 0x82, 0x83,
    0x84, 0x02, 0x5c, 0x03, 0xff, 0x02, 0x00, 0x04,
    0x41, 0x11, 0x01, 0x1a
};
static const guint8 CORE_INIT_RSP_NO_ROUTING[] = {
    0x40, 0x01, 0x19, 0x00, 0x03, 0x0e, 0x02, 0x00,
    0x08, 0x00, 0x01, 0x02, 0x03, 0x80, 0x82, 0x83,
    0x84, 0x02, 0x00, 0x00, 0xff, 0x02, 0x00, 0x04,
    0x41, 0x11, 0x01, 0x18
};
static const guint8 CORE_INIT_V2_RSP[] = {
    0x40, 0x01, 0x18, 0x00, 0x1a, 0x7e, 0x06, 0x00,
    0x02, 0x00, 0x02, 0xff, 0xff, 0x00, 0x0c, 0x01,
    0x05, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00,
    0x00, 0x90, 0x00
};
static const guint8 CORE_INIT_V2_RSP_NO_PROTOCOL_ROUTING[] = {
    0x40, 0x01, 0x18, 0x00, 0x1a, 0x7a, 0x06, 0x00,
    0x02, 0x00, 0x02, 0xff, 0xff, 0x00, 0x0c, 0x01,
    0x05, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00,
    0x00, 0x90, 0x00
};
static const guint8 CORE_INIT_V2_RSP_NO_TECHNOLOGY_ROUTING[] = {
    0x40, 0x01, 0x18, 0x00, 0x1a, 0x7c, 0x06, 0x00,
    0x02, 0x00, 0x02, 0xff, 0xff, 0x00, 0x0c, 0x01,
    0x05, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00,
    0x00, 0x90, 0x00
};
static const guint8 CORE_INIT_V2_RSP_NO_ROUTING[] = {
    0x40, 0x01, 0x18, 0x00, 0x1a, 0x70, 0x06, 0x00,
    0x02, 0x00, 0x02, 0xff, 0xff, 0x00, 0x0c, 0x01,
    0x05, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00,
    0x00, 0x90, 0x00
};
static const guint8 CORE_INIT_V2_RSP_ERROR[] = {
    0x40, 0x01, 0x0e, 0x03, 0x1a, 0x7e, 0x06, 0x00,
    0x02, 0x00, 0x02, 0xff, 0xff, 0x00, 0x0c, 0x01,
    0x00
};
static const guint8 CORE_INIT_V2_RSP_BROKEN1[] = {
    0x40, 0x01, 0x10, 0x00, 0x1a, 0x7e, 0x06, 0x00,
    0x02, 0x00, 0x02, 0xff, 0xff, 0x00, 0x0c, 0x01,
    0x05, 0x01, 0x00
};
static const guint8 CORE_INIT_V2_RSP_BROKEN2[] = {
    0x40, 0x01, 0x01, 0x03
};
static const guint8 CORE_INIT_RSP_BROKEN[] = {
    0x40, 0x01, 0x00
};
static const guint8 CORE_GET_CONFIG_CMD_RESET[] = {
    0x20, 0x03, 0x02, 0x01, 0x00 /* TOTAL_DURATION */
};
static const guint8 CORE_GET_CONFIG_RSP_WRONG_DURATION[] = {
    0x40, 0x03, 0x06, 0x00, 0x01, 0x00, 0x02, 0xfa,
    0x00
};
static const guint8 CORE_GET_CONFIG_RSP_DEFAULT_DURATION[] = {
    0x40, 0x03, 0x06, 0x00, 0x01, 0x00, 0x02, 0xf4,
    0x01
};
static const guint8 CORE_GET_CONFIG_RSP_DEFAULT_DURATION_EXTRA[] = {
    0x40, 0x03, 0x09, 0x00, 0x02, 0x50, 0x01, 0x02,
    0x00, 0x02, 0xf4, 0x01
};
static const guint8 CORE_GET_CONFIG_RSP_BROKEN_DURATION_1[] = {
    0x40, 0x03, 0x05, 0x00, 0x01, 0x00, 0x02, 0xf4
};
static const guint8 CORE_GET_CONFIG_RSP_BROKEN_DURATION_2[] = {
    0x40, 0x03, 0x05, 0x00, 0x01, 0x00, 0x01, 0xf4
};
static const guint8 CORE_GET_CONFIG_RSP_BROKEN_DURATION_3[] = {
    0x40, 0x03, 0x03, 0x00, 0x01, 0x00
};
static const guint8 CORE_GET_CONFIG_RSP_ERROR[] = {
    0x40, 0x03, 0x02, 0x03, 0x00
};
static const guint8 CORE_GET_CONFIG_RSP_INVALID_PARAM[] = {
    0x40, 0x03, 0x04, NCI_STATUS_INVALID_PARAM, 0x01, 0x11, 0x00
};
static const guint8 CORE_GET_CONFIG_BROKEN[] = {
    0x40, 0x03, 0x00
};
static const guint8 CORE_SET_CONFIG_CMD[] = {
    0x20, 0x02, 0x0b, 0x03, 0x00, 0x02, 0xf4, 0x01,
    0x08, 0x01, 0x00, 0x11, 0x01, 0x00
};
static const guint8 CORE_SET_CONFIG_RSP[] = {
    0x40, 0x02, 0x02, 0x00, 0x00
};
static const guint8 CORE_SET_CONFIG_RSP_ERROR[] = {
    0x40, 0x02, 0x02, NCI_STATUS_REJECTED, 0x00
};
static const guint8 CORE_GET_CONFIG_CMD_DISCOVERY[] = {
    0x20, 0x03, 0x03, 0x02, 0x32, 0x50 /* LA_SEL_INFO, LF_PROTOCOL_TYPE */
};
static const guint8 CORE_GET_CONFIG_RSP_DISCOVERY_INVALID_PARAM[] = {
    0x40, 0x03, 0x06, NCI_STATUS_INVALID_PARAM, 0x02,
    0x32, 0x00, 0x50, 0x00
};
static const guint8 CORE_GET_CONFIG_RSP_DISCOVERY_RW[] = {
    0x40, 0x03, 0x08, 0x00, 0x02,
    0x32, 0x01, 0x00, /* LA_SEL_INFO = 0 */
    0x50, 0x01, 0x00  /* LF_PROTOCOL_TYPE = 0 */
};
static const guint8 CORE_GET_CONFIG_RSP_DISCOVERY_PEER[] = {
    0x40, 0x03, 0x08, 0x00, 0x02,
    0x32, 0x01, 0x40, /* LA_SEL_INFO = 0x40 */
    0x50, 0x01, 0x02  /* LF_PROTOCOL_TYPE = 0x02 */
};
static const guint8 CORE_GET_CONFIG_RSP_DISCOVERY_CE[] = {
    0x40, 0x03, 0x08, 0x00, 0x02,
    0x32, 0x01, 0x20, /* LA_SEL_INFO = 0x20 */
    0x50, 0x01, 0x00  /* LF_PROTOCOL_TYPE = 0x00 */
};
static const guint8 CORE_GET_CONFIG_RSP_DISCOVERY_CE_PEER[] = {
    0x40, 0x03, 0x08, 0x00, 0x02,
    0x32, 0x01, 0x60, /* LA_SEL_INFO = 0x60 */
    0x50, 0x01, 0x02  /* LF_PROTOCOL_TYPE = 0x02 */
};
static const guint8 CORE_GET_CONFIG_RSP_DISCOVERY_CE_A[] = {
    0x40, 0x03, 0x08, 0x00, 0x02,
    0x32, 0x01, 0x60, /* LA_SEL_INFO = 0x60 */
    0x50, 0x01, 0x00  /* LF_PROTOCOL_TYPE = 0x00 */
};
static const guint8 CORE_SET_CONFIG_CMD_DISCOVERY_RW[] = {
    0x20, 0x02, 0x07, 0x02,
    0x32, 0x01, 0x00, /* LA_SEL_INFO = 0 */
    0x50, 0x01, 0x00  /* LF_PROTOCOL_TYPE = 0 */
};
static const guint8 CORE_SET_CONFIG_CMD_DISCOVERY_PEER[] = {
    0x20, 0x02, 0x07, 0x02,
    0x32, 0x01, 0x40, /* LA_SEL_INFO = 0x40 */
    0x50, 0x01, 0x02  /* LF_PROTOCOL_TYPE = 0x02 */
};
static const guint8 CORE_SET_CONFIG_CMD_DISCOVERY_CE[] = {
    0x20, 0x02, 0x07, 0x02,
    0x32, 0x01, 0x20, /* LA_SEL_INFO = 0x20 */
    0x50, 0x01, 0x00  /* LF_PROTOCOL_TYPE = 0x00 */
};
static const guint8 RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT[] = {
    0x21, 0x01, 0x07, 0x00, 0x01,
    0x00, 0x03, 0x00, 0x01, 0x00 /* NFC-A */
};
static const guint8 RF_SET_LISTEN_MODE_ROUTING_CMD_MIXED_RW_PEER[] = {
    0x21, 0x01, 0x2a, 0x00, 0x08,
    0x01, 0x03, 0x00, 0x01, 0x05, /* NFC-DEP */
    0x01, 0x03, 0x00, 0x01, 0x04, /* ISO-DEP */
    0x01, 0x03, 0x00, 0x01, 0x01, /* T1T */
    0x01, 0x03, 0x00, 0x01, 0x02, /* T2T */
    0x01, 0x03, 0x00, 0x01, 0x03, /* T3T */
    0x00, 0x03, 0x00, 0x01, 0x02, /* NFC-F */
    0x00, 0x03, 0x00, 0x01, 0x01, /* NFC-B */
    0x00, 0x03, 0x00, 0x01, 0x00  /* NFC-A */
};
static const guint8 RF_SET_LISTEN_MODE_ROUTING_CMD_MIXED_PEER[] = {
    0x21, 0x01, 0x16, 0x00, 0x04,
    0x01, 0x03, 0x00, 0x01, 0x05, /* NFC-DEP */
    0x00, 0x03, 0x00, 0x01, 0x02, /* NFC-F */
    0x00, 0x03, 0x00, 0x01, 0x01, /* NFC-B */
    0x00, 0x03, 0x00, 0x01, 0x00  /* NFC-A */
};
static const guint8 RF_SET_LISTEN_MODE_ROUTING_CMD_MIXED_A_B[] = {
    0x21, 0x01, 0x20, 0x00, 0x06,
    0x01, 0x03, 0x00, 0x01, 0x05, /* NFC-DEP */
    0x01, 0x03, 0x00, 0x01, 0x04, /* ISO-DEP */
    0x01, 0x03, 0x00, 0x01, 0x01, /* T1T */
    0x01, 0x03, 0x00, 0x01, 0x02, /* T2T */
    0x00, 0x03, 0x00, 0x01, 0x01, /* NFC-B */
    0x00, 0x03, 0x00, 0x01, 0x00  /* NFC-A */
};
static const guint8 RF_SET_LISTEN_MODE_ROUTING_CMD_TECHNOLOGY_B_A[] = {
    0x21, 0x01, 0x0c, 0x00, 0x02,
    0x00, 0x03, 0x00, 0x01, NCI_RF_TECHNOLOGY_B,
    0x00, 0x03, 0x00, 0x01, NCI_RF_TECHNOLOGY_A
};
static const guint8 RF_SET_LISTEN_MODE_ROUTING_CMD_TECHNOLOGY_F_B_A[] = {
    0x21, 0x01, 0x11, 0x00, 0x03,
    0x00, 0x03, 0x00, 0x01, NCI_RF_TECHNOLOGY_F,
    0x00, 0x03, 0x00, 0x01, NCI_RF_TECHNOLOGY_B,
    0x00, 0x03, 0x00, 0x01, NCI_RF_TECHNOLOGY_A
};
static const guint8 RF_SET_LISTEN_MODE_ROUTING_CMD_PROTOCOL_A_B[] = {
    0x21, 0x01, 0x16, 0x00, 0x04, 0x01, 0x03, 0x00,
    0x01, 0x01, 0x01, 0x03, 0x00, 0x01, 0x02, 0x01,
    0x03, 0x00, 0x01, 0x04, 0x01, 0x03, 0x00, 0x01,
    0x05
};
static const guint8 RF_SET_LISTEN_MODE_ROUTING_CMD_PROTOCOL_RW_PEER[] = {
    0x21, 0x01, 0x1b, 0x00, 0x05,
    0x01, 0x03, 0x00, 0x01, 0x05, /* NFC-DEP */
    0x01, 0x03, 0x00, 0x01, 0x04, /* ISO-DEP */
    0x01, 0x03, 0x00, 0x01, 0x01, /* T1T */
    0x01, 0x03, 0x00, 0x01, 0x02, /* T2T */
    0x01, 0x03, 0x00, 0x01, 0x03  /* T3T */
};
static const guint8 RF_SET_LISTEN_MODE_ROUTING_CMD_PROTOCOL_A_B_F[] = {
    0x21, 0x01, 0x1b, 0x00, 0x05, 0x01, 0x03, 0x00,
    0x01, 0x01, 0x01, 0x03, 0x00, 0x01, 0x02, 0x01,
    0x03, 0x00, 0x01, 0x04, 0x01, 0x03, 0x00, 0x01,
    0x03, 0x01, 0x03, 0x00, 0x01, 0x05
};
static const guint8 RF_SET_LISTEN_MODE_ROUTING_CMD_PROTOCOL_ISODEP[] = {
    0x21, 0x01, 0x07, 0x00, 0x01,
    0x01, 0x03, 0x00, 0x01, 0x04 /* ISO-DEP */
};
static const guint8 RF_SET_LISTEN_MODE_ROUTING_RSP[] = {
    0x41, 0x01, 0x01, 0x00
};
static const guint8 RF_SET_LISTEN_MODE_ROUTING_RSP_ERROR[] = {
    0x41, 0x01, 0x01, 0x01
};
static const guint8 RF_SET_LISTEN_MODE_ROUTING_RSP_BROKEN[] = {
    0x41, 0x01, 0x00
};
static const guint8 RF_DISCOVER_MAP_CMD_RW[] = {
    0x21, 0x00, 0x0d, 0x04,
    0x01, 0x01, 0x01, /* T1T/Poll/Frame */
    0x02, 0x01, 0x01, /* T2T/Poll/Frame */
    0x04, 0x01, 0x02, /* T3T/Poll/Frame */
    0x03, 0x01, 0x01  /* IsoDep/Poll/IsoDep */
};
static const guint8 RF_DISCOVER_MAP_CMD_RW_PEER[] = {
    0x21, 0x00, 0x13, 0x06,
    0x01, 0x01, 0x01, /* T1T/Poll/Frame */
    0x02, 0x01, 0x01, /* T2T/Poll/Frame */
    0x04, 0x01, 0x02, /* T3T/Poll/Frame */
    0x03, 0x01, 0x01, /* IsoDep/Poll/IsoDep */
    0x05, 0x01, 0x03, /* NfcDep/Poll/NfcDep */
    0x05, 0x02, 0x03  /* NfcDep/Listen/NfcDep */
};
static const guint8 RF_DISCOVER_MAP_CMD_CE_PEER[] = {
    0x21, 0x00, 0x16, 0x07,
    0x01, 0x01, 0x01, /* T1T/Poll/Frame */
    0x02, 0x01, 0x01, /* T2T/Poll/Frame */
    0x04, 0x01, 0x02, /* T3T/Poll/Frame */
    0x03, 0x01, 0x01, /* IsoDep/Poll/IsoDep */
    0x05, 0x01, 0x03, /* NfcDep/Poll/NfcDep */
    0x05, 0x02, 0x03, /* NfcDep/Listen/NfcDep */
    0x04, 0x02, 0x02  /* IsoDep/Listen/IsoDep */
};
static const guint8 RF_DISCOVER_MAP_CMD_POLL_NFCDEP[] = {
    0x21, 0x00, 0x0d, 0x04,
    0x01, 0x01, 0x01, /* T1T/Poll/Frame */
    0x02, 0x01, 0x01, /* T2T/Poll/Frame */
    0x04, 0x01, 0x02, /* T3T/Poll/Frame */
    0x05, 0x01, 0x03  /* NfcDep/Poll/NfcDep */
};
static const guint8 RF_DISCOVER_MAP_CMD_A_B_F[] = {
    0x21, 0x00, 0x10, 0x05, 0x01, 0x01, 0x01, 0x02,
    0x01, 0x01, 0x04, 0x01, 0x02, 0x03, 0x01, 0x01,
    0x05, 0x01, 0x03
};
static const guint8 RF_DISCOVER_MAP_CMD_A_B_F_NFCDEP[] = {
    0x21, 0x00, 0x16, 0x07, 0x01, 0x01, 0x01, 0x02,
    0x01, 0x01, 0x04, 0x01, 0x02, 0x03, 0x01, 0x01,
    0x05, 0x01, 0x03, 0x05, 0x02, 0x03, 0x04, 0x02,
    0x02
};
static const guint8 RF_DISCOVER_MAP_CMD_NFCDEP[] = {
    0x21, 0x00, 0x04, 0x01,
    0x05, 0x02, 0x03 /* NfcDep/Listen/NfcDep */
};
static const guint8 RF_DISCOVER_MAP_CMD_LISTEN_ISODEP[] = {
    0x21, 0x00, 0x04, 0x01,
    0x04, 0x02, 0x02 /* IsoDep/Listen/IsoDep */
};
static const guint8 RF_DISCOVER_MAP_CMD_ALL_A_B[] = {
    0x21, 0x00, 0x13, 0x06,
    0x01, 0x01, 0x01, /* T1T/Poll/Frame */
    0x02, 0x01, 0x01, /* T2T/Poll/Frame */
    0x04, 0x01, 0x02, /* IsoDep/Poll/IsoDep */
    0x05, 0x01, 0x03, /* NfcDep/Poll/NfcDep */
    0x05, 0x02, 0x03, /* NfcDep/Listen/NfcDep */
    0x04, 0x02, 0x02  /* IsoDep/Listen/IsoDep */
};
static const guint8 RF_DISCOVER_MAP_RSP[] = {
    0x41, 0x00, 0x01, 0x00
};
static const guint8 RF_DISCOVER_MAP_ERROR[] = {
    0x41, 0x00, 0x01, 0x03
};
static const guint8 RF_DISCOVER_MAP_BROKEN[] = {
    0x41, 0x00, 0x00
};
static const guint8 RF_DISCOVER_CMD_RW_A_B[] = {
    0x21, 0x03, 0x07, 0x03,
    0x03, 0x01, /* ActivePollA */
    0x00, 0x01, /* PassivePollA */
    0x01, 0x01, /* PassivePollB */
};
static const guint8 RF_DISCOVER_CMD_RW_A_B_F[] = {
    0x21, 0x03, 0x0b, 0x05,
    0x03, 0x01, /* ActivePollA */
    0x00, 0x01, /* PassivePollA */
    0x01, 0x01, /* PassivePollB */
    0x05, 0x01, /* ActivePollF */
    0x02, 0x01  /* PassivePollF */
};
static const guint8 RF_DISCOVER_CMD_RW_PEER_A_B_F[] = {
    0x21, 0x03, 0x0f, 0x07,
    0x03, 0x01, /* ActivePollA */
    0x00, 0x01, /* PassivePollA */
    0x01, 0x01, /* PassivePollB */
    0x05, 0x01, /* ActivePollF */
    0x02, 0x01, /* PassivePollF */
    0x80, 0x01, /* PassiveListenA */
    0x82, 0x01  /* PassiveListenF */
};
static const guint8 RF_DISCOVER_CMD_RW_CE_PEER_A_B_F[] = {
    0x21, 0x03, 0x13, 0x09,
    0x03, 0x01, /* ActivePollA */
    0x00, 0x01, /* PassivePollA */
    0x01, 0x01, /* PassivePollB */
    0x05, 0x01, /* ActivePollF */
    0x02, 0x01, /* PassivePollF */
    0x80, 0x01, /* PassiveListenA */
    0x83, 0x01, /* ActiveListenA */
    0x81, 0x01, /* PassiveListenB */
    0x82, 0x01  /* PassiveListenF */
};
static const guint8 RF_DISCOVER_CMD_ALL_A_B[] = {
    0x21, 0x03, 0x0d, 0x06,
    0x03, 0x01, /* ActivePollA */
    0x00, 0x01, /* PassivePollA */
    0x01, 0x01, /* PassivePollB */
    0x80, 0x01, /* PassiveListenA */
    0x83, 0x01, /* ActiveListenA */
    0x81, 0x01  /* PassiveListenB */
};
static const guint8 RF_DISCOVER_CMD_A_B_F[] = {
    0x21, 0x03, 0x0b, 0x05, 0x01, 0x01, 0x00, 0x01,
    0x03, 0x01, 0x02, 0x01, 0x05, 0x01
};
static const guint8 RF_DISCOVER_CMD_A_B_F_NFCDEP[] = {
    0x21, 0x03, 0x15, 0x0a, 0x01, 0x01, 0x00, 0x01,
    0x03, 0x01, 0x02, 0x01, 0x05, 0x01, 0x82, 0x01,
    0x85, 0x01, 0x80, 0x01, 0x83, 0x01, 0x81, 0x01
};
static const guint8 RF_DISCOVER_CMD_NFCDEP_LISTEN[] = {
    0x21, 0x03, 0x05, 0x02,
    0x80, 0x01, /* PassiveListenA */
    0x82, 0x01  /* PassiveListenF */
};
static const guint8 RF_DISCOVER_CMD_A_B_LISTEN[] = {
    0x21, 0x03, 0x07, 0x03,
    0x80, 0x01, /* PassiveListenA */
    0x83, 0x01, /* ActiveListenA */
    0x81, 0x01  /* PassiveListenB */

};
static const guint8 RF_DISCOVER_RSP[] = {
    0x41, 0x03, 0x01, 0x00
};
static const guint8 RF_DISCOVER_RSP_ERROR[] = {
    0x41, 0x03, 0x01, 0x03
};
static const guint8 RF_DISCOVER_RSP_BROKEN[] = {
    0x41, 0x03, 0x00
};
static const guint8 RF_INTF_ACTIVATED_NTF_T2[] = {
    0x61, 0x05, 0x17, 0x01, 0x01, 0x02, 0x00, 0xff,
    0x01, 0x0c, 0x44, 0x00, 0x07, 0x04, 0x9b, 0xfb,
    0x4a, 0xeb, 0x2b, 0x80, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00
};
static const guint8 RF_INTF_ACTIVATED_NTF_ISODEP[] = {
    0x61, 0x05, 0x1f, 0x01, 0x02, 0x04, 0x00, 0xff,
    0x01, 0x09, 0x04, 0x00, 0x04, 0x4f, 0x01, 0x74,
    0x01, 0x01, 0x20, 0x00, 0x00, 0x00, 0x0b, 0x0a,
    0x78, 0x80, 0x81, 0x02, 0x4b, 0x4f, 0x4e, 0x41,
    0x14, 0x11
};
static const guint8 RF_INTF_ACTIVATED_NTF_T2_BROKEN1[] = {
    0x61, 0x05, 0x05 /* Too short */, 0x01, 0x01, 0x02, 0x00, 0xff
};
static const guint8 RF_INTF_ACTIVATED_NTF_T2_BROKEN2[] = {
    0x61, 0x05, 0x14 /* Still too short */, 0x01, 0x01, 0x02, 0x00, 0xff,
    0x01, 0x0c, 0x44, 0x00, 0x07, 0x04, 0x9b, 0xfb,
    0x4a, 0xeb, 0x2b, 0x80, 0x01, 0x00, 0x00
};
static const guint8 RF_INTF_ACTIVATED_NTF_T2_BROKEN3[] = {
    0x61, 0x05, 0x0b, 0x01, 0x01, 0x02, 0x00, 0xff,
    0x01, 0x00 /* Missing RF Tech Parameters */, 0x00, 0x00, 0x00, 0x00
};
static const guint8 RF_INTF_ACTIVATED_NTF_T4A[] = {
    0x61, 0x05, 0x27, 0x01, 0x02, 0x04, 0x00, 0xff,
    0x01, 0x09, 0x04, 0x00, 0x04, 0x37, 0xf4, 0x95,
    0x95, 0x01, 0x20, 0x00, 0x00, 0x00, 0x13, 0x12,
    0x78, 0x80, 0x72, 0x02, 0x80, 0x31, 0x80, 0x66,
    0xb1, 0x84, 0x0c, 0x01, 0x6e, 0x01, 0x83, 0x00,
    0x90, 0x00
};
static const guint8 RF_INTF_ACTIVATED_NTF_T4A_BROKEN1[] = {
    0x61, 0x05, 0x27, 0x01, 0x02, 0x04, 0x00, 0xff,
    0x01, 0x09, 0x04, 0x00, 0x04, 0x37, 0xf4, 0x95,
    0x95, 0x01, 0x20, 0x00, 0x00, 0x00, 0x14 /* One too many */, 0x12,
    0x78, 0x80, 0x72, 0x02, 0x80, 0x31, 0x80, 0x66,
    0xb1, 0x84, 0x0c, 0x01, 0x6e, 0x01, 0x83, 0x00,
    0x90, 0x00
};
static const guint8 RF_INTF_ACTIVATED_NTF_T4A_BROKEN_ACT_PARAM1[] = {
    0x61, 0x05, 0x27, 0x01, 0x02, 0x04, 0x00, 0xff,
    0x01, 0x09, 0x04, 0x00, 0x04, 0x37, 0xf4, 0x95,
    0x95, 0x01, 0x20, 0x00, 0x00, 0x00, 0x13, 0x13 /* One too many */,
    0x78, 0x80, 0x72, 0x02, 0x80, 0x31, 0x80, 0x66,
    0xb1, 0x84, 0x0c, 0x01, 0x6e, 0x01, 0x83, 0x00,
    0x90, 0x00
};
static const guint8 RF_INTF_ACTIVATED_NTF_T4A_BROKEN_ACT_PARAM2[] = {
    0x61, 0x05, 0x15, 0x01, 0x02, 0x04, 0x00, 0xff,
    0x01, 0x09, 0x04, 0x00, 0x04, 0x37, 0xf4, 0x95,
    0x95, 0x01, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00 /* Missing params */,
};
static const guint8 RF_INTF_ACTIVATED_NTF_NFCDEP_LISTEN_A[] = {
    0x61, 0x05, 0x2e, 0x01, 0x03, 0x05, 0x83, 0xfb,
    0x01, 0x00, 0x85, 0x02, 0x02, 0x23, 0x22, 0x10,
    0x5a, 0x37, 0xa5, 0x7b, 0x88, 0x6e, 0x6e, 0xef,
    0x45, 0x00, 0x00, 0x00, 0x32, 0x46, 0x66, 0x6d,
    0x01, 0x01, 0x12, 0x02, 0x02, 0x07, 0xff, 0x03,
    0x02, 0x00, 0x13, 0x04, 0x01, 0x64, 0x07, 0x01,
    0x03
};
static const guint8 RF_INTF_ACTIVATED_NTF_CE_A[] = {
    0x61, 0x05, 0x0c, 0x01, 0x02, 0x04, 0x80, 0xff,
    0x01, 0x00, 0x80, 0x00, 0x00, 0x01, 0x80
};
static const guint8 RF_DEACTIVATE_IDLE_CMD[] = {
    0x21, 0x06, 0x01, 0x00
};
static const guint8 RF_DEACTIVATE_DISCOVERY_CMD[] = {
    0x21, 0x06, 0x01, 0x03
};
static const guint8 RF_DEACTIVATE_BAD_CMD[] = {
    0x21, 0x06, 0x01, 0x04
};
static const guint8 RF_DEACTIVATE_RSP[] = {
    0x41, 0x06, 0x01, 0x00
};
static const guint8 RF_DEACTIVATE_RSP_ERROR[] = {
    0x41, 0x06, 0x01, 0x03
};
static const guint8 RF_DEACTIVATE_RSP_BROKEN[] = {
    0x41, 0x06, 0x00
};
static const guint8 RF_DEACTIVATE_NTF_IDLE[] = {
    0x61, 0x06, 0x02, 0x00, 0x00
};
static const guint8 RF_DEACTIVATE_NTF_DISCOVERY[] = {
    0x61, 0x06, 0x02, 0x03, 0x00
};
static const guint8 RF_DEACTIVATE_NTF_DISCOVERY_EP_REQUEST[] = {
    0x61, 0x06, 0x02, 0x03, 0x01
};
static const guint8 RF_DEACTIVATE_NTF_SLEEP_EP_REQUEST[] = {
    0x61, 0x06, 0x02, 0x01, 0x01
};
static const guint8 RF_DEACTIVATE_NTF_SLEEP_AF_EP_REQUEST[] = {
    0x61, 0x06, 0x02, 0x02, 0x01
};
static const guint8 RF_DEACTIVATE_NTF_UNSUPPORTED[] = {
    0x61, 0x06, 0x02, 0x04, 0x01
};
static const guint8 RF_DEACTIVATE_NTF_BROKEN[] = {
    0x61, 0x06, 0x00
};
static const guint8 CORE_GENERIC_ERROR_NTF[] = {
    0x60, 0x07, 0x01, NCI_STATUS_FAILED
};
static const guint8 CORE_GENERIC_TARGET_ACTIVATION_FAILED_ERROR_NTF[] = {
    0x60, 0x07, 0x01, NCI_DISCOVERY_TARGET_ACTIVATION_FAILED
};
static const guint8 CORE_GENERIC_TEAR_DOWN_ERROR_NTF[] = {
    0x60, 0x07, 0x01, NCI_DISCOVERY_TEAR_DOWN
};
static const guint8 CORE_GENERIC_ERROR_NTF_BROKEN[] = {
    0x60, 0x07, 0x00
};
static const guint8 CORE_CONN_CREDITS_NTF[] = {
    0x60, 0x06, 0x03, 0x01, 0x00, 0x01
};
static const guint8 CORE_CONN_CREDITS_BROKEN1_NTF[] = {
    0x60, 0x06, 0x00
};
static const guint8 CORE_CONN_CREDITS_BROKEN2_NTF[] = {
    0x60, 0x06, 0x02, 0x01, 0x00
};
static const guint8 CORE_INTERFACE_GENERIC_ERROR_NTF[] = {
    0x60, 0x08, 0x02, NCI_STATUS_FAILED, 0x00
};
static const guint8 CORE_INTERFACE_SYNTAX_ERROR_NTF[] = {
    0x60, 0x08, 0x02, NCI_STATUS_SYNTAX_ERROR, 0x00
};
static const guint8 CORE_INTERFACE_TRANSMISSION_ERROR_NTF[] = {
    0x60, 0x08, 0x02, NCI_RF_TRANSMISSION_ERROR, 0x00
};
static const guint8 CORE_INTERFACE_PROTOCOL_ERROR_NTF[] = {
    0x60, 0x08, 0x02, NCI_RF_PROTOCOL_ERROR, 0x00
};
static const guint8 CORE_INTERFACE_TIMEOUT_ERROR_NTF[] = {
    0x60, 0x08, 0x02, NCI_RF_TIMEOUT_ERROR, 0x00
};
static const guint8 CORE_INTERFACE_ERROR_NTF_BROKEN[] = {
    0x60, 0x08, 0x01, NCI_STATUS_FAILED
};
static const guint8 CORE_IGNORED_NTF[] = {
    0x60, 0x33, 0x00
};
static const guint8 RF_IGNORED_NTF[] = {
    0x61, 0x33, 0x00
};
static const guint8 NFCEE_IGNORED_NTF[] = {
    0x62, 0x33, 0x00
};
static const guint8 RF_DISCOVER_NTF_1_ISODEP[] = {
    0x61, 0x03, 0x0e, 0x01, 0x04, 0x00, 0x09, 0x04,
    0x00, 0x04, 0x4f, 0x01, 0x74, 0x01, 0x01, 0x20,
    0x02
};
static const guint8 RF_DISCOVER_NTF_1_T2T[] = {
    0x61, 0x03, 0x0e, 0x01, 0x02, 0x00, 0x09, 0x04,
    0x00, 0x04, 0x4f, 0x01, 0x74, 0x01, 0x01, 0x08,
    0x02
};
static const guint8 RF_DISCOVER_NTF_2_T2T[] = {
    0x61, 0x03, 0x0e, 0x02, 0x02, 0x00, 0x09, 0x04,
    0x00, 0x04, 0x4f, 0x01, 0x74, 0x01, 0x01, 0x08,
    0x02
};
static const guint8 RF_DISCOVER_NTF_PROPRIETARY_MORE[] = {
    0x61, 0x03, 0x0e, 0x02, 0x80, 0x00, 0x09, 0x04,
    0x00, 0x04, 0x4f, 0x01, 0x74, 0x01, 0x01, 0x08,
    0x02
};
static const guint8 RF_DISCOVER_NTF_2_PROPRIETARY_LAST[] = {
    0x61, 0x03, 0x0e, 0x02, 0x80, 0x00, 0x09, 0x04,
    0x00, 0x04, 0x4f, 0x01, 0x74, 0x01, 0x01, 0x08,
    0x00
};
static const guint8 RF_DISCOVER_NTF_3_PROPRIETARY_LAST[] = {
    0x61, 0x03, 0x0e, 0x03, 0x81, 0x00, 0x09, 0x04,
    0x00, 0x04, 0x4f, 0x01, 0x74, 0x01, 0x01, 0x08,
    0x00
};
static const guint8 RF_DISCOVER_SELECT_1_T2T_CMD[] = {
    0x21, 0x04, 0x03, 0x01, 0x02, 0x01
};
static const guint8 RF_DISCOVER_SELECT_1_ISODEP_CMD[] = {
    0x21, 0x04, 0x03, 0x01, 0x04, 0x02
};
static const guint8 RF_DISCOVER_SELECT_RSP[] = {
    0x41, 0x04, 0x01, 0x00
};

static
void
test_bytes_unref(
    gpointer bytes)
{
    g_bytes_unref(bytes);
}

static
void
test_data_packet_handler_not_reached(
    NciCore* nci,
    guint8 cid,
    const void* payload,
    guint len,
    void* user_data)
{
    g_assert_not_reached();
}

/*==========================================================================*
 * Dummy HAL
 *==========================================================================*/

static
gboolean
test_dummy_hal_io_start(
    NciHalIo* io,
    NciHalClient* client)
{
    return FALSE;
}

static
void
test_dummy_hal_io_stop(
    NciHalIo* io)
{
}

static
gboolean
test_dummy_hal_io_write(
    NciHalIo* io,
    const GUtilData* chunks,
    guint count,
    NciHalClientFunc complete)
{
    return FALSE;
}

static
void
test_dummy_hal_io_cancel_write(
    NciHalIo* io)
{
}

static const NciHalIoFunctions test_dummy_hal_io_fn = {
    .start = test_dummy_hal_io_start,
    .stop = test_dummy_hal_io_stop,
    .write = test_dummy_hal_io_write,
    .cancel_write = test_dummy_hal_io_cancel_write
};

static NciHalIo test_dummy_hal_io = {
    .fn = &test_dummy_hal_io_fn
};

/*==========================================================================*
 * Test HAL
 *==========================================================================*/

typedef struct test_hal_io {
    NciHalIo io;
    GPtrArray* read_queue;
    GPtrArray* cmd_expected;
    guint write_id;
    guint read_id;
    NciHalClient* sar;
    void* test_data;
    guint fail_write;
    guint rsp_expected;
    GMainLoop* write_flush_loop;
} TestHalIo;

typedef struct test_hal_io_read {
    TestHalIo* hal;
    GBytes* bytes;
    gboolean spontaneous;
} TestHalIoRead;

typedef struct test_hal_io_write {
    TestHalIo* hal;
    GBytes* bytes;
    NciHalClientFunc complete;
} TestHalIoWrite;

static
void
test_hal_io_read_free(
    gpointer user_data)
{
    TestHalIoRead* read = user_data;

    g_bytes_unref(read->bytes);
    g_free(read);
}

static
void
test_hal_io_write_free(
    gpointer user_data)
{
    TestHalIoWrite* write = user_data;

    g_bytes_unref(write->bytes);
    g_free(write);
}

static
gboolean
test_hal_io_next_rsp(
    TestHalIo* hal)
{
    GPtrArray* queue = hal->read_queue;

    if (queue && queue->len && hal->sar) {
        TestHalIoRead* read = queue->pdata[0];

        return !read->spontaneous;
    }
    return FALSE;
}

static
gboolean
test_hal_io_next_ntf(
    TestHalIo* hal)
{
    GPtrArray* queue = hal->read_queue;

    if (queue && queue->len && hal->sar) {
        TestHalIoRead* read = queue->pdata[0];

        return read->spontaneous;
    }
    return FALSE;
}

static
void
test_hal_io_read_one(
    TestHalIo* hal)
{
    GPtrArray* queue = hal->read_queue;
    TestHalIoRead* read = queue->pdata[0];
    GBytes* bytes = g_bytes_ref(read->bytes);
    gsize len;
    const void* data = g_bytes_get_data(bytes, &len);

    g_ptr_array_remove_index(queue, 0);
    hal->sar->fn->read(hal->sar, data, len);
    g_bytes_unref(bytes);
}

static
void
test_hal_io_flush_ntf(
    TestHalIo* hal)
{
    while (test_hal_io_next_ntf(hal)) {
        test_hal_io_read_one(hal);
    }
}

static
gboolean
test_hal_io_read_cb(
    gpointer user_data)
{
    TestHalIo* hal = user_data;

    hal->read_id = 0;
    test_hal_io_flush_ntf(hal);
    if (hal->rsp_expected && test_hal_io_next_rsp(hal)) {
        hal->rsp_expected--;
        test_hal_io_read_one(hal);
        test_hal_io_flush_ntf(hal);
    }
    return G_SOURCE_REMOVE;
}

static
void
test_dump_bytes(
    char dir,
    GBytes* bytes)
{
    gsize len;
    const guint8* data = g_bytes_get_data(bytes, &len);
    char buf[GUTIL_HEXDUMP_BUFSIZE];

    while (len > 0) {
        const guint consumed = gutil_hexdump(buf, data, len);

        gutil_log(NULL, GLOG_LEVEL_DEBUG, "%c %s", dir, buf);
        data += consumed;
        len -= consumed;
        dir = ' ';
    }
}

static
gboolean
test_hal_io_write_cb(
    gpointer user_data)
{
    TestHalIoWrite* write = user_data;
    TestHalIo* hal = write->hal;

    g_assert(hal->write_id);
    if (hal->cmd_expected->len) {
        GBytes* expected = hal->cmd_expected->pdata[0];

        if (!g_bytes_equal(expected, write->bytes)) {
            GDEBUG("Unexpected write:");
            test_dump_bytes('<', write->bytes);
            GDEBUG("Doesn't match this:");
            test_dump_bytes('<', expected);
            g_assert_not_reached();
        }
        g_ptr_array_remove_index(hal->cmd_expected, 0);
    } else {
        GDEBUG("Unchecked write");
        test_dump_bytes('<', write->bytes);
    }

    hal->write_id = 0;
    hal->rsp_expected++;
    if (write->complete) {
        write->complete(hal->sar, TRUE);
    }
    if (!hal->read_id) {
        hal->read_id = g_idle_add(test_hal_io_read_cb, hal);
    }
    if (!hal->write_id && hal->write_flush_loop) {
        GDEBUG("Unblocking flush waiter");
        test_quit_later(hal->write_flush_loop);
        hal->write_flush_loop = NULL;
    }
    return G_SOURCE_REMOVE;
}

static
gboolean
test_hal_io_start(
    NciHalIo* io,
    NciHalClient* sar)
{
    TestHalIo* hal = G_CAST(io, TestHalIo, io);

    g_assert(!hal->read_id);
    g_assert(!hal->sar);
    hal->sar = sar;
    if (test_hal_io_next_ntf(hal)) {
        hal->read_id = g_idle_add(test_hal_io_read_cb, hal);
    }
    return TRUE;
}
static
void
test_hal_io_stop(
    NciHalIo* io)
{
    TestHalIo* hal = G_CAST(io, TestHalIo, io);

    g_assert(hal->sar);
    hal->sar = NULL;
    if (hal->read_queue) {
        g_ptr_array_set_size(hal->read_queue, 0);
    }
    if (hal->read_id) {
        g_source_remove(hal->read_id);
        hal->read_id = 0;
    }
    if (hal->write_id) {
        g_source_remove(hal->write_id);
        hal->write_id = 0;
    }
}

static
gboolean
test_hal_io_write(
    NciHalIo* io,
    const GUtilData* chunks,
    guint count,
    NciHalClientFunc complete)
{
    TestHalIo* hal = G_CAST(io, TestHalIo, io);

    if (hal->fail_write) {
        hal->fail_write--;
        GDEBUG("Simulating write failure");
        return FALSE;
    } else {
        TestHalIoWrite* write = g_new0(TestHalIoWrite, 1);
        GByteArray* packet = g_byte_array_new();
        guint i;

        g_assert(count > 0);
        for (i = 0; i < count; i++) {
            g_byte_array_append(packet, chunks[i].bytes, chunks[i].size);
        }

        g_assert(hal->sar);
        g_assert(!hal->write_id);
        write->hal = hal;
        write->bytes = g_byte_array_free_to_bytes(packet);
        write->complete = complete;
        hal->write_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
            test_hal_io_write_cb, write, test_hal_io_write_free);
        return TRUE;
    }
}

static
void
test_hal_io_cancel_write(
    NciHalIo* io)
{
    TestHalIo* hal = G_CAST(io, TestHalIo, io);

    g_assert(hal->write_id);
    g_source_remove(hal->write_id);
    hal->write_id = 0;
}

static
TestHalIo*
test_hal_io_new_with_functions(
    const NciHalIoFunctions* fn)
{
    TestHalIo* hal = g_new0(TestHalIo, 1);

    hal->io.fn = fn;
    hal->cmd_expected = g_ptr_array_new();
    g_ptr_array_set_free_func(hal->cmd_expected, test_bytes_unref);
    return hal;
}

static
TestHalIo*
test_hal_io_new(
    void)
{
    static const NciHalIoFunctions test_hal_io_fn = {
        .start = test_hal_io_start,
        .stop = test_hal_io_stop,
        .write = test_hal_io_write,
        .cancel_write = test_hal_io_cancel_write
    };

    return test_hal_io_new_with_functions(&test_hal_io_fn);
}

static
void
test_hal_io_queue_read(
    TestHalIo* hal,
    const void* data,
    guint len,
    gboolean spontaneous)
{
    TestHalIoRead* read = g_new0(TestHalIoRead, 1);

    if (!hal->read_queue) {
        hal->read_queue = g_ptr_array_new();
        g_ptr_array_set_free_func(hal->read_queue, test_hal_io_read_free);
    }

    read->hal = hal;
    read->bytes = g_bytes_new(data, len);
    read->spontaneous = spontaneous;
    g_ptr_array_add(hal->read_queue, read);
    if (!hal->read_id && test_hal_io_next_ntf(hal)) {
        hal->read_id = g_idle_add(test_hal_io_read_cb, hal);
    }
}

#define test_hal_io_queue_rsp(hal,rsp) \
    test_hal_io_queue_read(hal, TEST_ARRAY_AND_SIZE(rsp), FALSE)
#define test_hal_io_queue_ntf(hal,ntf) \
    test_hal_io_queue_read(hal, TEST_ARRAY_AND_SIZE(ntf), TRUE)

static
void
test_hal_io_free(
   TestHalIo* hal)
{
    g_assert(!hal->sar);
    g_assert(!hal->write_id);
    g_assert(!hal->cmd_expected->len);
    g_ptr_array_free(hal->cmd_expected, TRUE);
    if (hal->read_queue) {
        g_ptr_array_free(hal->read_queue, TRUE);
    }
    g_free(hal);
}

/*==========================================================================*
 * null
 *==========================================================================*/

static
void
test_null(
    void)
{
    NciCore* nci = nci_core_new(&test_dummy_hal_io);

    g_assert(!nci_core_new(NULL));
    g_assert(!nci_core_send_data_msg(NULL, 0, NULL, NULL, NULL, NULL));
    g_assert(!nci_core_add_current_state_changed_handler(NULL, NULL, NULL));
    g_assert(!nci_core_add_next_state_changed_handler(NULL, NULL, NULL));
    g_assert(!nci_core_add_intf_activated_handler(NULL, NULL, NULL));
    g_assert(!nci_core_add_data_packet_handler(NULL, NULL, NULL));

    g_assert(!nci_core_add_current_state_changed_handler(nci, NULL, NULL));
    g_assert(!nci_core_add_next_state_changed_handler(nci, NULL, NULL));
    g_assert(!nci_core_add_intf_activated_handler(nci, NULL, NULL));
    g_assert(!nci_core_add_data_packet_handler(nci, NULL, NULL));
    nci_core_remove_handler(nci, 0);

    nci_core_set_state(NULL, NCI_STATE_INIT);
    nci_core_set_op_mode(NULL, NFC_OP_MODE_NONE);
    nci_core_cancel(NULL, 0);
    nci_core_remove_handler(NULL, 0);
    nci_core_restart(NULL);
    nci_core_free(NULL);

    nci_core_free(nci);
}

/*==========================================================================*
 * restart
 *==========================================================================*/

static
void
test_restart_done(
    NciCore* nci,
    void* user_data)
{
    g_assert(nci->current_state == NCI_RFST_IDLE);
    test_quit_later((GMainLoop*)user_data);
}

static
void
test_restart(
    void)
{
    TestHalIo* hal = test_hal_io_new();
    NciCore* nci = nci_core_new(&hal->io);
    GMainLoop* loop = g_main_loop_new(NULL, TRUE);
    gulong id;

    nci->cmd_timeout = (test_opt.flags & TEST_FLAG_DEBUG) ? 0 :
        TEST_DEFAULT_CMD_TIMEOUT;

    /* Responses */
    test_hal_io_queue_rsp(hal, CORE_RESET_RSP);
    test_hal_io_queue_ntf(hal, CORE_IGNORED_NTF);
    test_hal_io_queue_rsp(hal, CORE_INIT_RSP);
    test_hal_io_queue_rsp(hal, CORE_GET_CONFIG_RSP_DEFAULT_DURATION);

    id = nci_core_add_current_state_changed_handler(nci,
        test_restart_done, loop);
    nci_core_restart(nci);
    test_run_loop(&test_opt, loop);
    nci_core_remove_handler(nci, id);

    g_assert(nci->current_state == NCI_RFST_IDLE);
    g_assert(nci->next_state == NCI_RFST_IDLE);

    /* Again with a cancel */
    nci_core_set_state(nci, NCI_RFST_DISCOVERY);
    GDEBUG("Starting all over again");
    nci_core_restart(nci); /* Cancels the above request */
    test_hal_io_queue_rsp(hal, CORE_RESET_RSP);
    test_hal_io_queue_ntf(hal, CORE_IGNORED_NTF);
    test_hal_io_queue_rsp(hal, CORE_INIT_RSP);
    test_hal_io_queue_rsp(hal, CORE_GET_CONFIG_RSP_DEFAULT_DURATION);

    id = nci_core_add_current_state_changed_handler(nci,
        test_restart_done, loop);
    test_run_loop(&test_opt, loop);
    nci_core_remove_handler(nci, id);

    g_assert(nci->current_state == NCI_RFST_IDLE);
    g_assert(nci->next_state == NCI_RFST_IDLE);

    nci_core_free(nci);
    test_hal_io_free(hal);
    g_main_loop_unref(loop);
}

/*==========================================================================*
 * init_ok
 *==========================================================================*/

static
void
test_init_ok_done(
    NciCore* nci,
    void* user_data)
{
    g_assert(nci->current_state == NCI_RFST_IDLE);
    test_quit_later((GMainLoop*)user_data);
}

static
void
test_init_ok(
    void)
{
    TestHalIo* hal = test_hal_io_new();
    NciCore* nci = nci_core_new(&hal->io);
    GMainLoop* loop = g_main_loop_new(NULL, TRUE);
    gulong id[2];

    /* Second time does nothing */
    nci_core_set_state(nci, NCI_RFST_IDLE);
    nci_core_set_state(nci, NCI_RFST_IDLE);
    /* Couple of error notifications (ignored) */
    test_hal_io_queue_ntf(hal, CORE_GENERIC_ERROR_NTF_BROKEN);
    test_hal_io_queue_ntf(hal, CORE_GENERIC_ERROR_NTF);
    /* Responses */
    test_hal_io_queue_rsp(hal, CORE_RESET_RSP);
    test_hal_io_queue_ntf(hal, CORE_IGNORED_NTF);
    /* Couple more broken notifications */
    test_hal_io_queue_ntf(hal, CORE_CONN_CREDITS_BROKEN1_NTF);
    test_hal_io_queue_ntf(hal, CORE_CONN_CREDITS_BROKEN2_NTF);
    /* Final response */
    test_hal_io_queue_rsp(hal, CORE_INIT_RSP);
    /* SET_CONFIG errors are ignored */
    test_hal_io_queue_rsp(hal, CORE_GET_CONFIG_RSP_ERROR);
    /* SET_CONFIG errors are ignored too */
    test_hal_io_queue_rsp(hal, CORE_SET_CONFIG_RSP_ERROR);
    /* And a valid notification */
    test_hal_io_queue_ntf(hal, CORE_CONN_CREDITS_NTF);
    /* Two more error notifications (ignored) */
    test_hal_io_queue_ntf(hal, CORE_GENERIC_ERROR_NTF_BROKEN);
    test_hal_io_queue_ntf(hal, CORE_GENERIC_ERROR_NTF);

    nci_core_set_op_mode(nci, NFC_OP_MODE_RW | NFC_OP_MODE_POLL);
    id[0] = nci_core_add_current_state_changed_handler(nci,
        test_init_ok_done, loop);
    id[1] = nci_core_add_data_packet_handler(nci,
        test_data_packet_handler_not_reached, NULL);

    g_assert(id[0]);
    g_assert(id[1]);

    test_run_loop(&test_opt, loop);

    g_assert(nci->current_state == NCI_RFST_IDLE);
    g_assert(nci->next_state == NCI_RFST_IDLE);
    nci_core_remove_all_handlers(nci, id);
    g_assert(!id[0]);
    g_assert(!id[1]);
    nci_core_free(nci);
    test_hal_io_free(hal);
    g_main_loop_unref(loop);
}

/*==========================================================================*
 * init_failed1
 *==========================================================================*/

static
void
test_init_failed1_done(
    NciCore* nci,
    void* user_data)
{
    g_assert(nci->current_state == NCI_STATE_ERROR);
    test_quit_later((GMainLoop*)user_data);
}

static
void
test_init_failed1(
    void)
{
    NciCore* nci = nci_core_new(&test_dummy_hal_io);
    GMainLoop* loop = g_main_loop_new(NULL, TRUE);
    gulong id;

    nci_core_set_state(nci, NCI_RFST_IDLE);
    id = nci_core_add_current_state_changed_handler(nci,
        test_init_failed1_done, loop);

    test_run_loop(&test_opt, loop);

    g_assert(nci->current_state == NCI_STATE_ERROR);
    g_assert(nci->next_state == NCI_STATE_ERROR);
    nci_core_remove_handler(nci, id);
    nci_core_free(nci);
    g_main_loop_unref(loop);
}

/*==========================================================================*
 * init_failed2
 *==========================================================================*/

static
void
test_init_failed2_done(
    NciCore* nci,
    void* user_data)
{
    g_assert(nci->current_state == NCI_STATE_ERROR);
    test_quit_later((GMainLoop*)user_data);
}

static
void
test_init_failed2(
    void)
{
    TestHalIo* hal = test_hal_io_new();
    NciCore* nci;
    GMainLoop* loop;
    gulong id;

    hal->fail_write++;
    nci = nci_core_new(&hal->io);
    loop = g_main_loop_new(NULL, TRUE);
    nci_core_set_state(nci, NCI_RFST_IDLE);
    id = nci_core_add_current_state_changed_handler(nci,
        test_init_failed2_done, loop);

    test_run_loop(&test_opt, loop);

    g_assert(nci->current_state == NCI_STATE_ERROR);
    g_assert(nci->next_state == NCI_STATE_ERROR);
    nci_core_remove_handler(nci, id);
    nci_core_free(nci);
    test_hal_io_free(hal);
    g_main_loop_unref(loop);
}

/*==========================================================================*
 * nci_sm
 *==========================================================================*/

typedef struct test_nci_sm_entry TestSmEntry;
typedef struct test_nci_sm_entry_timeout TestSmEntryTimeout;
typedef struct test_nci_sm_entry_send_data TestSmEntrySendData;
typedef struct test_nci_sm_entry_queue_read TestSmEntryQueueRead;
typedef struct test_nci_sm_entry_assert_states TestSmEntryAssertStates;
typedef struct test_nci_sm_entry_state TestSmEntryState;
typedef struct test_nci_sm_entry_activation TestEntryActivation;

typedef struct test_nci_sm_data {
    const char* name;
    const TestSmEntry* entries;
    const char* config;
} TestNciSmData;

typedef struct test_nci_sm {
    TestHalIo* hal;
    NciCore* nci;
    GMainLoop* loop;
    const TestNciSmData* data;
    const TestSmEntry* entry;
} TestNciSm;

struct test_nci_sm_entry {
    void (*func)(TestNciSm* test);
    union {
        struct test_nci_sm_entry_timeout {
            guint ms;
        } timeout;
        struct test_nci_sm_entry_send_data {
            const void* data;
            guint len;
            guint8 cid;
        } send_data;
        GUtilData expect_cmd;
        struct test_nci_sm_entry_queue_read {
            const void* data;
            guint len;
            gboolean ntf;
        } queue_read;
        struct test_nci_sm_entry_assert_states {
            NCI_STATE current_state;
            NCI_STATE next_state;
        } assert_states;
        struct test_nci_sm_entry_state {
            NCI_STATE state;
        } state;
        struct test_nci_sm_entry_op_mode {
            NCI_OP_MODE op_mode;
        } op_mode;
        struct test_nci_sm_entry_activation {
            NCI_RF_INTERFACE rf_intf;
            NCI_PROTOCOL protocol;
            NCI_MODE mode;
        } activation;
    } data;
};

#define TEST_NCI_SM_SET_TIMEOUT(millis) { \
    .func = test_nci_sm_set_timeout, \
    .data.timeout = { .ms = millis } }
#define TEST_NCI_SM_RF_SEND(bytes) { \
    .func = test_nci_sm_send_data, \
    .data.send_data = { .data = bytes, .len = sizeof(bytes), \
        .cid = NCI_STATIC_RF_CONN_ID } }
#define TEST_NCI_SM_QUEUE_RSP(bytes) { \
    .func = test_nci_sm_queue_read, \
    .data.queue_read = { .data = bytes, .len = sizeof(bytes), .ntf = FALSE } }
#define TEST_NCI_SM_QUEUE_NTF(bytes) { \
    .func = test_nci_sm_queue_read, \
    .data.queue_read = { .data = bytes, .len = sizeof(bytes), .ntf = TRUE } }
#define TEST_NCI_SM_EXPECT_CMD(write_data) { \
    .func = test_nci_sm_expect_cmd, \
    .data.expect_cmd = { .bytes = write_data, .size = sizeof(write_data) } }
#define TEST_NCI_SM_ASSERT_STATES(current,next) { \
    .func = test_nci_sm_assert_states, \
    .data.assert_states = { .current_state = current, .next_state = next } }
#define TEST_NCI_SM_ASSERT_STATE(state) { \
    .func = test_nci_sm_assert_states, \
    .data.assert_states = { .current_state = state, .next_state = state } }
#define TEST_NCI_SM_SET_STATE(next_state) { \
    .func = test_nci_sm_set_state, \
    .data.state = { .state = next_state } }
#define TEST_NCI_SM_SET_OP_MODE(mode) { \
    .func = test_nci_sm_set_op_mode, \
    .data.op_mode = { .op_mode = mode } }
#define TEST_NCI_SM_SYNC() { \
    .func = test_nci_sm_sync }
#define TEST_NCI_SM_WAIT_STATE(wait_state) { \
    .func = test_nci_sm_wait_state, \
    .data.state = { .state = wait_state } }
#define TEST_NCI_SM_WAIT_ACTIVATION(intf,prot,mod) { \
    .func = test_nci_sm_wait_activation, \
    .data.activation = { .rf_intf = intf, .protocol = prot, .mode = mod } }
#define TEST_NCI_SM_END() { .func = NULL }

static
void
test_nci_sm_set_timeout(
    TestNciSm* test)
{
    NciCore* nci = test->nci;
    const TestSmEntryTimeout* timeout = &test->entry->data.timeout;

    GDEBUG("Timeout %u ms", timeout->ms);
    nci->cmd_timeout = timeout->ms;
}

static
void
test_nci_sm_set_op_mode(
    TestNciSm* test)
{
    const NCI_OP_MODE op_mode = test->entry->data.op_mode.op_mode;

    GDEBUG("OpMode 0x%02x", op_mode);
    nci_core_set_op_mode(test->nci, op_mode);
}

static
void
test_nci_sm_send_data_cb(
    NciCore* nci,
    gboolean success,
    void* user_data)
{
    g_assert(success);
}

static
void
test_nci_sm_send_data(
    TestNciSm* test)
{
    const TestSmEntrySendData* send = &test->entry->data.send_data;
    GBytes* bytes = g_bytes_new(send->data, send->len);

    g_assert(nci_core_send_data_msg(test->nci, send->cid, bytes,
        test_nci_sm_send_data_cb, test_bytes_unref, bytes));
}

static
void
test_nci_sm_queue_read(
    TestNciSm* test)
{
    const TestSmEntryQueueRead* queue_read = &test->entry->data.queue_read;

    test_hal_io_queue_read(test->hal, queue_read->data, queue_read->len,
        queue_read->ntf);
}

static
void
test_nci_sm_expect_cmd(
    TestNciSm* test)
{
    const GUtilData* expected = &test->entry->data.expect_cmd;

    g_ptr_array_add(test->hal->cmd_expected,
        g_bytes_new_static(expected->bytes, expected->size));
}

static
void
test_nci_sm_assert_states(
    TestNciSm* test)
{
    NciCore* nci = test->nci;
    const TestSmEntryAssertStates* assert_states =
        &test->entry->data.assert_states;

    g_assert_cmpint(assert_states->current_state, == ,nci->current_state);
    g_assert_cmpint(assert_states->next_state, == ,nci->next_state);
}

static
void
test_nci_sm_set_state(
    TestNciSm* test)
{
    nci_core_set_state(test->nci, test->entry->data.state.state);
}

static
void
test_nci_sm_sync(
    TestNciSm* test)
{
    TestHalIo* hal = test->hal;

    while (hal->write_id || hal->cmd_expected->len > 0) {
        if (hal->write_id) {
            GDEBUG("Waiting for pending write to complete");
        } else {
            GDEBUG("Waiting for expected command to be submitted");
        }
        hal->write_flush_loop = test->loop;
        g_main_loop_run(test->loop);
    }
}

static
void
test_nci_sm_wait_state_cb(
    NciCore* nci,
    void* user_data)
{
    TestNciSm* test = user_data;
    const TestSmEntryState* wait = &test->entry->data.state;

    GDEBUG("Current state %d", nci->current_state);
    g_assert(nci == test->nci);
    if (nci->current_state == wait->state) {
        test_quit_later(test->loop);
    }
}

static
void
test_nci_sm_wait_state(
    TestNciSm* test)
{
    NciCore* nci = test->nci;
    const TestSmEntryState* wait = &test->entry->data.state;

    test_hal_io_flush_ntf(test->hal);
    if (nci->current_state == wait->state) {
        GDEBUG("No need to wait, the state is already %d", wait->state);
    } else {
        gulong id = nci_core_add_current_state_changed_handler(nci,
            test_nci_sm_wait_state_cb, test);

        GDEBUG("Waiting for state %d", wait->state);
        g_main_loop_run(test->loop);
        nci_core_remove_handler(nci, id);
        g_assert(nci->current_state == wait->state);
    }
}

static
void
test_nci_sm_wait_activation_cb(
    NciCore* nci,
    const NciIntfActivationNtf* ntf,
    void* user_data)
{
    TestNciSm* test = user_data;
    const TestEntryActivation* wait = &test->entry->data.activation;

    GDEBUG("Activation %u/%u/%u", ntf->rf_intf, ntf->protocol, ntf->mode);
    g_assert(nci == test->nci);
    if (ntf->rf_intf == wait->rf_intf &&
        ntf->protocol == wait->protocol &&
        ntf->mode == wait->mode) {
        test_quit_later(test->loop);
    }
}

static
void
test_nci_sm_wait_activation(
    TestNciSm* test)
{
    NciCore* nci = test->nci;
    const TestEntryActivation* wait = &test->entry->data.activation;
    gulong id = nci_core_add_intf_activated_handler(nci,
        test_nci_sm_wait_activation_cb, test);

    GDEBUG("Waiting for %u/%u/%u activation", wait->rf_intf,
        wait->protocol, wait->mode);
    test_hal_io_flush_ntf(test->hal);
    g_main_loop_run(test->loop);
    nci_core_remove_handler(nci, id);
}

static
void
test_nci_sm(
    gconstpointer test_data)
{
    TestNciSm test;
    guint timeout_id;
    char* dir = NULL;
    char* conf = NULL;
    const TestNciSmData* data = test_data;
    const char* default_conf = nci_sm_config_file;

    if (data->config) {
        dir = g_dir_make_tmp(TMP_DIR_TEMPLATE, NULL);
        nci_sm_config_file = conf = g_build_filename(dir, "test.conf", NULL);
        g_assert(g_file_set_contents(conf, data->config, -1, NULL));
        GDEBUG("Wrote %s", conf);
    }

    memset(&test, 0, sizeof(test));
    test.hal = test_hal_io_new();
    test.nci = nci_core_new(&test.hal->io);
    test.loop = g_main_loop_new(NULL, TRUE);
    test.data = data;
    test.entry = test.data->entries;

    if (test_opt.flags & TEST_FLAG_DEBUG) {
        test.nci->cmd_timeout = 0;
        timeout_id = 0;
    } else {
        test.nci->cmd_timeout = TEST_DEFAULT_CMD_TIMEOUT;
        timeout_id = test_setup_timeout(&test_opt);
    }

    while (test.entry->func) {
        test.entry->func(&test);
        test.entry++;
    }

    if (timeout_id) {
        g_source_remove(timeout_id);
    }

    if (test.data->config) {
        nci_sm_config_file = default_conf;
        remove(conf);
        g_free(conf);
        remove(dir);
        g_free(dir);
    }

    nci_core_free(test.nci);
    test_hal_io_free(test.hal);
    g_main_loop_unref(test.loop);
}

/* State machine tests */

static const TestSmEntry test_nci_sm_init_ok_default_duration[] = {
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_ok_no_duration[] = {
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_WRONG_DURATION),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_ok_wrong_duration[] = {
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_WRONG_DURATION),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_ok_broken_duration1[] = {
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_BROKEN_DURATION_1),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_ok_broken_duration2[] = {
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_BROKEN_DURATION_2),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_ok_broken_duration3[] = {
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_BROKEN_DURATION_3),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_timeout[] = {
    TEST_NCI_SM_SET_TIMEOUT(TEST_SHORT_TIMEOUT),
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    /* No CORE_INIT_RSP */
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_reset_timeout[] = {
    TEST_NCI_SM_SET_TIMEOUT(TEST_SHORT_TIMEOUT),
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    /* No CORE_RESET_RSP */
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_get_config_timeout[] = {
    TEST_NCI_SM_SET_TIMEOUT(TEST_SHORT_TIMEOUT),
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    /* No CORE_GET_CONFIG_RSP */
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_set_config_timeout[] = {
    TEST_NCI_SM_SET_TIMEOUT(TEST_SHORT_TIMEOUT),
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_WRONG_DURATION),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD),
    /* No CORE_SET_CONFIG_RSP */
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_v2[] = {
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V2),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_WRONG_DURATION),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_v2_error[] = {
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V2),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_ERROR),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_v2_broken1[] = {
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V2),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_BROKEN1),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_v2_broken2[] = {
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V2),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_BROKEN2),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_v2_timeout[] = {
    TEST_NCI_SM_SET_TIMEOUT(TEST_SHORT_TIMEOUT),
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V2),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_reset_v2_failed[] = {
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP_ERROR),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_reset_failed[] = {
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP_ERROR),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_RSP), /* Ignored */
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_reset_broken[] = {
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP_BROKEN),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_broken[] = {
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF), /* Unexpected (ignored) */
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP_BROKEN),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_get_config_error[] = {
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    /* GET_CONFIG errors are ignored */
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_INVALID_PARAM),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_get_config_broken[] = {
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_BROKEN),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_ignore_unexpected_rsp[] = {
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP), /* Unexpected (ignored) */
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    /* This GET_CONFIG_RSP contains default TOTAL_DURATION + extra parameter */
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION_EXTRA),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_no_routing[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP_NO_ROUTING),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    /* Not configuring routing because Max Routing Table Size is 0 */

    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_invalid_param[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_INVALID_PARAM),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD_DISCOVERY_RW),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),  /* Peer modes off */
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),

    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_get_config_error[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_ERROR),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD_DISCOVERY_RW),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),  /* Peer modes off */
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),

    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_set_config_error[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_PEER),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD_DISCOVERY_RW),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP_ERROR), /* Continuing anyway */
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),

    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_failed[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP_ERROR),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_broken[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP_BROKEN),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_v2[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V2),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_NO_ROUTING),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_PEER),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD_DISCOVERY_RW),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),  /* Peer modes off */
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),

    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_v2_protocol[] = {
    TEST_NCI_SM_SET_OP_MODE(NFC_OP_MODE_RW|NFC_OP_MODE_PEER|
                            NFC_OP_MODE_POLL|NFC_OP_MODE_LISTEN),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V2),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD_DISCOVERY_PEER),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),  /* Peer modes on */
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_MIXED_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_PEER_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_v2_mixed_error[] = {
    TEST_NCI_SM_SET_OP_MODE(NFC_OP_MODE_RW|NFC_OP_MODE_PEER|
                            NFC_OP_MODE_POLL|NFC_OP_MODE_LISTEN),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V2),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD_DISCOVERY_PEER),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),  /* Peer modes on */
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_MIXED_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP_ERROR),
    /* Mixed routing failed, try protocol routing */
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_PROTOCOL_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_PEER_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_v2_protocol_error[] = {
    TEST_NCI_SM_SET_OP_MODE(NFC_OP_MODE_RW|NFC_OP_MODE_PEER|
                            NFC_OP_MODE_POLL|NFC_OP_MODE_LISTEN),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V2),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_NO_TECHNOLOGY_ROUTING),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_PEER),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_PROTOCOL_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP_ERROR),
    /* Ignoring the error and continuing anyway */
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_PEER_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_v2_technology1[] = {
    TEST_NCI_SM_SET_OP_MODE(NFC_OP_MODE_RW|NFC_OP_MODE_PEER|
                            NFC_OP_MODE_POLL|NFC_OP_MODE_LISTEN),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V2),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_PEER),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_MIXED_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP_ERROR),
    /* Mixed routing failed, trying protocol routing */
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_PROTOCOL_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP_ERROR),
    /* Protocol routing failed too, resort to technology */
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_TECHNOLOGY_F_B_A),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_PEER_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_v2_technology2[] = {
    TEST_NCI_SM_SET_OP_MODE(NFC_OP_MODE_RW|NFC_OP_MODE_PEER|
                            NFC_OP_MODE_POLL|NFC_OP_MODE_LISTEN),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V2),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_NO_PROTOCOL_ROUTING),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_PEER),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_TECHNOLOGY_F_B_A),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_PEER_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_v2_technology3[] = {
    TEST_NCI_SM_SET_OP_MODE(NFC_OP_MODE_RW|NFC_OP_MODE_PEER|
                            NFC_OP_MODE_POLL|NFC_OP_MODE_LISTEN),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V2),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_NO_PROTOCOL_ROUTING),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_PEER),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_TECHNOLOGY_F_B_A),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP_ERROR),
    /* Ignore the error and continue anyway */
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_PEER_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_v2_technology4[] = {
    TEST_NCI_SM_SET_OP_MODE(NFC_OP_MODE_RW|NFC_OP_MODE_PEER|
                            NFC_OP_MODE_POLL|NFC_OP_MODE_LISTEN),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V2),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_NO_PROTOCOL_ROUTING),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_PEER),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_TECHNOLOGY_F_B_A),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP_BROKEN),
    /* Ignore the error and continue anyway */
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_PEER_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discover_map_error[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_ERROR),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discover_map_broken[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_BROKEN),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_idle_discovery[] = {
    TEST_NCI_SM_SET_OP_MODE(NFC_OP_MODE_RW|NFC_OP_MODE_PEER|
                            NFC_OP_MODE_POLL|NFC_OP_MODE_LISTEN),
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_IDLE),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_PEER),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_MIXED_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_PEER_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_GENERIC_ERROR_NTF),  /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_IGNORED_NTF),        /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_QUEUE_NTF(RF_IGNORED_NTF),          /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(NFCEE_IGNORED_NTF),       /* Ignored */
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* And then switch back to IDLE */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_DISCOVERY, NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_IGNORED_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* And again to DISCOVERY */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_PEER),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_MIXED_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_PEER_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_idle_discovery2[] = {
    TEST_NCI_SM_SET_OP_MODE(NFC_OP_MODE_RW|NFC_OP_MODE_PEER|
                            NFC_OP_MODE_POLL|NFC_OP_MODE_LISTEN),
    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_IDLE),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_PEER),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_MIXED_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_PEER_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_GENERIC_ERROR_NTF),  /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_IGNORED_NTF),        /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_QUEUE_NTF(RF_IGNORED_NTF),          /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(NFCEE_IGNORED_NTF),       /* Ignored */
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Setting the same mode has no effect */
    TEST_NCI_SM_SET_OP_MODE(NFC_OP_MODE_RW|NFC_OP_MODE_PEER|
                            NFC_OP_MODE_POLL|NFC_OP_MODE_LISTEN),
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_DISCOVERY, NCI_RFST_DISCOVERY),

    /* But changing the mode switches SM back to IDLE */
    TEST_NCI_SM_SET_OP_MODE(NFC_OP_MODE_PEER|NFC_OP_MODE_POLL),
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_DISCOVERY, NCI_RFST_IDLE),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_IGNORED_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_idle_failed[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* And then switch back to IDLE (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_DISCOVERY, NCI_RFST_IDLE),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_ERROR),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_idle_broken[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* And then switch back to IDLE (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_DISCOVERY, NCI_RFST_IDLE),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_BROKEN),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_idle_timeout[] = {
    TEST_NCI_SM_SET_TIMEOUT(TEST_SHORT_TIMEOUT),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* And then switch back to IDLE (and timeout) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_DISCOVERY, NCI_RFST_IDLE),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_poll_idle[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_QUEUE_NTF(CORE_IGNORED_NTF),        /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(NFCEE_IGNORED_NTF),       /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to IDLE */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_QUEUE_NTF(CORE_IGNORED_NTF),        /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(RF_IGNORED_NTF),          /* Ignored */
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_IDLE),
    TEST_NCI_SM_QUEUE_NTF(RF_IGNORED_NTF),          /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(NFCEE_IGNORED_NTF),       /* Ignored */
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_poll_idle_failed[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to IDLE (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_ERROR),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_poll_idle_broken1[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to IDLE (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_BROKEN),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_poll_idle_broken2[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to IDLE (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_BROKEN),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_poll_discovery[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* These notification don't switch the state */
    TEST_NCI_SM_QUEUE_NTF(CORE_GENERIC_TARGET_ACTIVATION_FAILED_ERROR_NTF),
    TEST_NCI_SM_QUEUE_NTF(CORE_GENERIC_TEAR_DOWN_ERROR_NTF),
    TEST_NCI_SM_QUEUE_NTF(CORE_GENERIC_ERROR_NTF),
    TEST_NCI_SM_QUEUE_NTF(CORE_GENERIC_ERROR_NTF_BROKEN),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to DISCOVERY */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_IGNORED_NTF),  /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(RF_IGNORED_NTF),  /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(NFCEE_IGNORED_NTF),  /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Another activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then to IDLE */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_IDLE),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_dscvr_error1[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to DISCOVERY (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_ERROR), /* Fail to Discovery */

    /* Switch to to Idle instead */
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_dscvr_error2[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to DISCOVERY (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_ERROR), /* Fail to Discovery */
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_ERROR), /* Fail to Idle */
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_dscvr_error3[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to DISCOVERY (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_ERROR), /* Fail to Discovery */
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_BROKEN), /* Fail to Idle */
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_dscvr_error4[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* Receive unsupported RF_DEACTIVATE_NTF */
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_UNSUPPORTED),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_dscvr_broken[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to DISCOVERY (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_ERROR), /* Fail to Discovery */
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_ERROR), /* Fail to Idle */
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const guint8 READ_CMD[] = {
    0x00, 0x00, 0x02, 0x30, 0x00
};

static const guint8 READ_RESP[] = {
    0x00, 0x00, 0x11, 0x04, 0x9b, 0xfb, 0xec, 0x4a,
    0xeb, 0x2b, 0x80, 0x0a, 0x48, 0x00, 0x00, 0xe1,
    0x10, 0x12, 0x00, 0x00,
};

static const TestSmEntry test_nci_sm_dscvr_poll_read_dscvr[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* Read sector 0 */
    TEST_NCI_SM_RF_SEND(READ_CMD),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_QUEUE_NTF(READ_RESP),

    /* Deactivate to DISCOVERY */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_deact_t4a[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Activation 1 */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T4A),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_ISO_DEP,
        NCI_PROTOCOL_ISO_DEP, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* Make sure these notifications don't change the state */
    TEST_NCI_SM_QUEUE_NTF(RF_IGNORED_NTF),                   /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_ERROR_NTF_BROKEN),  /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_GENERIC_ERROR_NTF), /* Ignored */
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* This one does */
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_TRANSMISSION_ERROR_NTF),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Activation 2 */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T4A),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_ISO_DEP,
        NCI_PROTOCOL_ISO_DEP, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* Back to DISCOVERY */
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_PROTOCOL_ERROR_NTF),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Activation 3 */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T4A),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_ISO_DEP,
        NCI_PROTOCOL_ISO_DEP, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* Back to DISCOVERY */
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_TIMEOUT_ERROR_NTF),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Activation 4 */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T4A),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_ISO_DEP,
        NCI_PROTOCOL_ISO_DEP, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* Back to DISCOVERY */
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_SYNTAX_ERROR_NTF),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Activation 5 */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T4A),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_ISO_DEP,
        NCI_PROTOCOL_ISO_DEP, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* Deactivate to IDLE */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_POLL_ACTIVE, NCI_RFST_IDLE),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_IDLE),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_act_error1[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Broken activation switches state machine to DISCOVERY */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2_BROKEN1),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_act_error2[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Broken activation switches state machine to DISCOVERY */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2_BROKEN2),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_act_error3[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Broken activation switches state machine to DISCOVERY */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2_BROKEN3),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_act_error4[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Broken activation switches state machine to DISCOVERY */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T4A_BROKEN1),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_deact_t4a_badparam1[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T4A_BROKEN_ACT_PARAM1),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_ISO_DEP,
        NCI_PROTOCOL_ISO_DEP, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* Deactivate to IDLE */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_POLL_ACTIVE, NCI_RFST_IDLE),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_IDLE),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_deact_t4a_badparam2[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Activation (missing activation parameters, not a fatal error) */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T4A_BROKEN_ACT_PARAM2),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_ISO_DEP,
        NCI_PROTOCOL_ISO_DEP, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_deact_timeout[] = {
    TEST_NCI_SM_SET_TIMEOUT(TEST_SHORT_TIMEOUT),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to DISCOVERY (and timeout) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_ntf_t2t[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Receive 2 discovery notifications (T2T, Proprietary) */
    TEST_NCI_SM_QUEUE_NTF(RF_DISCOVER_NTF_1_T2T),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_W4_ALL_DISCOVERIES),
    TEST_NCI_SM_QUEUE_NTF(CORE_IGNORED_NTF),        /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(RF_IGNORED_NTF),          /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(RF_DISCOVER_NTF_2_PROPRIETARY_LAST),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_W4_HOST_SELECT),

    /* Select Type 2 interface */
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_SELECT_1_T2T_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_SELECT_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_ntf_broken[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Receive 3 discovery notifications (T2T, ISO-DEP, Proprietary) */
    TEST_NCI_SM_QUEUE_NTF(RF_DISCOVER_NTF_1_T2T),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_W4_ALL_DISCOVERIES),
    TEST_NCI_SM_QUEUE_NTF(RF_DISCOVER_NTF_2_PROPRIETARY_LAST),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_W4_HOST_SELECT),

    /* Select Type 2 interface */
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_SELECT_1_T2T_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_SELECT_RSP),

    /* Broken notification switches us back to DISCOVERY */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2_BROKEN1),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_ntf_isodep[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Receive 2 discovery notifications (ISO-DEP, T2T and Proprietary) */
    TEST_NCI_SM_QUEUE_NTF(RF_DISCOVER_NTF_1_ISODEP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_W4_ALL_DISCOVERIES),
    TEST_NCI_SM_QUEUE_NTF(RF_DISCOVER_NTF_2_T2T),
    TEST_NCI_SM_QUEUE_NTF(RF_DISCOVER_NTF_3_PROPRIETARY_LAST),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_W4_HOST_SELECT),

    /* Make sure these notifications don't change the state */
    TEST_NCI_SM_QUEUE_NTF(RF_IGNORED_NTF),                   /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_IGNORED_NTF),                 /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(NFCEE_IGNORED_NTF),                /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_GENERIC_ERROR_NTF),           /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_GENERIC_ERROR_NTF_BROKEN),    /* Ignored */
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_W4_HOST_SELECT),

    /* Select ISO-DEP interface */
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_SELECT_1_ISODEP_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_SELECT_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_IGNORED_NTF),  /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(RF_IGNORED_NTF),    /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_ISODEP),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_ISO_DEP,
        NCI_PROTOCOL_ISO_DEP, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* Try to deactivate to DISCOVERY */
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_TIMEOUT_ERROR_NTF),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),

    /* But end up in IDLE */
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_IDLE),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_ntf_isodep_fail[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Receive 2 discovery notifications (ISO-DEP, and Proprietary) */
    TEST_NCI_SM_QUEUE_NTF(RF_DISCOVER_NTF_1_ISODEP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_W4_ALL_DISCOVERIES),
    TEST_NCI_SM_QUEUE_NTF(RF_DISCOVER_NTF_2_PROPRIETARY_LAST),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_SELECT_1_ISODEP_CMD),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_W4_HOST_SELECT),
    TEST_NCI_SM_SYNC(),

    /* Activation failure sends us back to DISCOVERY state */
    TEST_NCI_SM_QUEUE_NTF(CORE_GENERIC_TARGET_ACTIVATION_FAILED_ERROR_NTF),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_ntf_noselect[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Receive 2 discovery notifications (both Proprietary) */
    TEST_NCI_SM_QUEUE_NTF(RF_DISCOVER_NTF_PROPRIETARY_MORE),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_W4_ALL_DISCOVERIES),
    TEST_NCI_SM_QUEUE_NTF(RF_DISCOVER_NTF_2_PROPRIETARY_LAST),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_W4_HOST_SELECT),

    /* State machine will deactivate to DISCOVERY via IDLE */
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_IDLE_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_nfc_dep_listen_disappear[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_DEFAULT),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_RW),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* NFC-DEP peer appears and disappears */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_NFCDEP_LISTEN_A),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_NFC_DEP,
        NCI_PROTOCOL_NFC_DEP, NCI_MODE_ACTIVE_LISTEN_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_LISTEN_ACTIVE),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY_EP_REQUEST),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_nfc_dep_listen_timeout[] = {
    TEST_NCI_SM_SET_OP_MODE(NFC_OP_MODE_PEER|NFC_OP_MODE_LISTEN),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD_DISCOVERY_PEER),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),  /* Peer modes on */
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_MIXED_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_NFCDEP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_NFCDEP_LISTEN),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* NFC-DEP peer appears */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_NFCDEP_LISTEN_A),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_NFC_DEP,
        NCI_PROTOCOL_NFC_DEP, NCI_MODE_ACTIVE_LISTEN_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_LISTEN_ACTIVE),

    /* Notifications unsupported by this state are ignored */
    TEST_NCI_SM_QUEUE_NTF(NFCEE_IGNORED_NTF),
    TEST_NCI_SM_QUEUE_NTF(CORE_IGNORED_NTF),
    TEST_NCI_SM_QUEUE_NTF(RF_IGNORED_NTF),
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_GENERIC_ERROR_NTF),
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_ERROR_NTF_BROKEN),
    TEST_NCI_SM_ASSERT_STATE(NCI_RFST_LISTEN_ACTIVE),

    /* Error notification moves state machine back to DISCOVERY */
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_TIMEOUT_ERROR_NTF),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* The same with other errors */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_NFCDEP_LISTEN_A),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_NFC_DEP,
        NCI_PROTOCOL_NFC_DEP, NCI_MODE_ACTIVE_LISTEN_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_LISTEN_ACTIVE),
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_PROTOCOL_ERROR_NTF),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_LISTEN_ACTIVE),
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_LISTEN_ACTIVE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_NFCDEP_LISTEN_A),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_NFC_DEP,
        NCI_PROTOCOL_NFC_DEP, NCI_MODE_ACTIVE_LISTEN_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_LISTEN_ACTIVE),
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_TRANSMISSION_ERROR_NTF),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_NFCDEP_LISTEN_A),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_NFC_DEP,
        NCI_PROTOCOL_NFC_DEP, NCI_MODE_ACTIVE_LISTEN_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_LISTEN_ACTIVE),
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_SYNTAX_ERROR_NTF),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_nfc_dep_listen_sleep[] = {
    TEST_NCI_SM_SET_OP_MODE(NFC_OP_MODE_PEER|NFC_OP_MODE_LISTEN),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_PEER),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_MIXED_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_NFCDEP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_NFCDEP_LISTEN),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* NFC-DEP peer appears */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_NFCDEP_LISTEN_A),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_NFC_DEP,
        NCI_PROTOCOL_NFC_DEP, NCI_MODE_ACTIVE_LISTEN_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_LISTEN_ACTIVE),

    /* Sleep, EP Request */
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_SLEEP_EP_REQUEST),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_LISTEN_SLEEP),
    TEST_NCI_SM_QUEUE_NTF(CORE_IGNORED_NTF), /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(RF_IGNORED_NTF),   /* Ignored */
    TEST_NCI_SM_ASSERT_STATE(NCI_RFST_LISTEN_SLEEP),

    /* NFC-DEP re-appears */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_NFCDEP_LISTEN_A),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_NFC_DEP,
        NCI_PROTOCOL_NFC_DEP, NCI_MODE_ACTIVE_LISTEN_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_LISTEN_ACTIVE),

    /* Sleep_AF, EP Request */
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_SLEEP_AF_EP_REQUEST),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_LISTEN_SLEEP),

    /* Deactivate to discovery */
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_iso_dep_ce_tech_routing[] = {
    TEST_NCI_SM_SET_OP_MODE(NFC_OP_MODE_CE|NFC_OP_MODE_LISTEN),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V2),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_NO_PROTOCOL_ROUTING),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD_DISCOVERY_CE),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_TECHNOLOGY_B_A),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_LISTEN_ISODEP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_A_B_LISTEN),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* NFC-A/ISO-DEP Listen Mode activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_CE_A),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_ISO_DEP,
        NCI_PROTOCOL_ISO_DEP, NCI_MODE_PASSIVE_LISTEN_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_LISTEN_ACTIVE),

    /* Error moves state machine back to DISCOVERY */
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_TIMEOUT_ERROR_NTF),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_iso_dep_ce_prot_routing[] = {
    TEST_NCI_SM_SET_OP_MODE(NFC_OP_MODE_CE|NFC_OP_MODE_LISTEN),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V2),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_NO_TECHNOLOGY_ROUTING),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_RW),
    TEST_NCI_SM_EXPECT_CMD(CORE_SET_CONFIG_CMD_DISCOVERY_CE),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_PROTOCOL_ISODEP),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_LISTEN_ISODEP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_A_B_LISTEN),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* NFC-A/ISO-DEP Listen Mode activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_CE_A),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_ISO_DEP,
        NCI_PROTOCOL_ISO_DEP, NCI_MODE_PASSIVE_LISTEN_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_LISTEN_ACTIVE),

    /* Error moves state machine back to DISCOVERY */
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_TIMEOUT_ERROR_NTF),
    TEST_NCI_SM_EXPECT_CMD(RF_DEACTIVATE_DISCOVERY_CMD),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_config_abf[] = {
    TEST_NCI_SM_SET_OP_MODE(NFC_OP_MODE_RW|NFC_OP_MODE_PEER|NFC_OP_MODE_CE|
                            NFC_OP_MODE_POLL|NFC_OP_MODE_LISTEN),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_CE_PEER),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_MIXED_RW_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_CE_PEER),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_RW_CE_PEER_A_B_F),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_config_ab[] = {
    TEST_NCI_SM_SET_OP_MODE(NFC_OP_MODE_RW|NFC_OP_MODE_PEER|NFC_OP_MODE_CE|
                            NFC_OP_MODE_POLL|NFC_OP_MODE_LISTEN),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),

    TEST_NCI_SM_EXPECT_CMD(CORE_RESET_CMD),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_INIT_CMD_V1),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_RESET),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DEFAULT_DURATION),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_EXPECT_CMD(CORE_GET_CONFIG_CMD_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_DISCOVERY_CE_A),
    TEST_NCI_SM_EXPECT_CMD(RF_SET_LISTEN_MODE_ROUTING_CMD_MIXED_A_B),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_MAP_CMD_ALL_A_B),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_EXPECT_CMD(RF_DISCOVER_CMD_ALL_A_B),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const char test_nci_config_ab_data_default[] = CONFIG_SECTION "\n";
static const char test_nci_config_ab_data_junk[] = "junk";
static const char test_nci_config_ab_data[] =
    CONFIG_SECTION "\n"
    CONFIG_ENTRY_TECHNOLOGIES " = A,B\n";
static const char test_nci_config_ab_data_x[] =
    CONFIG_SECTION "\n"
    CONFIG_ENTRY_TECHNOLOGIES " = A,B,X\n"; /* X is ignored */

static const TestNciSmData nci_sm_tests[] = {
    { "init-ok-no-duration", test_nci_sm_init_ok_no_duration },
    { "init-ok-default-duration", test_nci_sm_init_ok_default_duration },
    { "init-ok-wrong-duration", test_nci_sm_init_ok_wrong_duration },
    { "init-ok-broken-duration1", test_nci_sm_init_ok_broken_duration1 },
    { "init-ok-broken-duration2", test_nci_sm_init_ok_broken_duration2 },
    { "init-ok-broken-duration3", test_nci_sm_init_ok_broken_duration3 },
    { "init-timeout", test_nci_sm_init_timeout },
    { "reset-timeout", test_nci_sm_reset_timeout },
    { "get-config-timeout", test_nci_sm_get_config_timeout },
    { "set-config-timeout", test_nci_sm_set_config_timeout },
    { "init-v2", test_nci_sm_init_v2 },
    { "init-v2-error", test_nci_sm_init_v2_error },
    { "init-v2-broken1", test_nci_sm_init_v2_broken1 },
    { "init-v2-broken2", test_nci_sm_init_v2_broken2 },
    { "init-v2-timeout", test_nci_sm_init_v2_timeout },
    { "reset-v2-failed", test_nci_sm_reset_v2_failed },
    { "reset-failed", test_nci_sm_reset_failed },
    { "reset-broken", test_nci_sm_reset_broken },
    { "init-broken", test_nci_sm_init_broken },
    { "get-config-error", test_nci_sm_get_config_error },
    { "get-config-broken", test_nci_sm_get_config_broken },
    { "ignore-unexpected-rsp", test_nci_sm_ignore_unexpected_rsp },
    { "discovery-no-routing", test_nci_sm_discovery_no_routing },
    { "discovery-invalid-param", test_nci_sm_discovery_invalid_param },
    { "discovery-get-config-error", test_nci_sm_discovery_get_config_error },
    { "discovery-set-config-error", test_nci_sm_discovery_set_config_error },
    { "discovery-failed", test_nci_sm_discovery_failed },
    { "discovery-broken", test_nci_sm_discovery_broken },
    { "discovery-v2", test_nci_sm_discovery_v2 },
    { "discovery-v2-protocol", test_nci_sm_discovery_v2_protocol },
    { "discovery-v2-mixed-error", test_nci_sm_discovery_v2_mixed_error },
    { "discovery-v2-protocol-error", test_nci_sm_discovery_v2_protocol_error },
    { "discovery-v2-technology1", test_nci_sm_discovery_v2_technology1 },
    { "discovery-v2-technology2", test_nci_sm_discovery_v2_technology2 },
    { "discovery-v2-technology3", test_nci_sm_discovery_v2_technology3 },
    { "discovery-v2-technology4", test_nci_sm_discovery_v2_technology4 },
    { "discovery-discover-map-error", test_nci_sm_discover_map_error },
    { "discovery-discover-map-broken", test_nci_sm_discover_map_broken },
    { "discovery-idle-discovery", test_nci_sm_discovery_idle_discovery },
    { "discovery-idle-discovery2", test_nci_sm_discovery_idle_discovery2 },
    { "discovery-idle-failed", test_nci_sm_discovery_idle_failed },
    { "discovery-idle-broken", test_nci_sm_discovery_idle_broken },
    { "discovery-idle-timeout", test_nci_sm_discovery_idle_timeout },
    { "discovery-poll-idle", test_nci_sm_discovery_poll_idle },
    { "discovery-poll-idle-failed", test_nci_sm_discovery_poll_idle_failed },
    { "discovery-poll-idle-broken1", test_nci_sm_discovery_poll_idle_broken1 },
    { "discovery-poll-idle-broken2", test_nci_sm_discovery_poll_idle_broken2 },
    { "discovery-poll-discovery", test_nci_sm_discovery_poll_discovery },
    { "discovery-poll-discovery-error1", test_nci_sm_dscvr_poll_dscvr_error1 },
    { "discovery-poll-discovery-error2", test_nci_sm_dscvr_poll_dscvr_error2 },
    { "discovery-poll-discovery-error3", test_nci_sm_dscvr_poll_dscvr_error3 },
    { "discovery-poll-discovery-error4", test_nci_sm_dscvr_poll_dscvr_error4 },
    { "discovery-poll-discovery-broken", test_nci_sm_dscvr_poll_dscvr_broken },
    { "discovery-poll-read-discovery", test_nci_sm_dscvr_poll_read_dscvr },
    { "discovery-poll-deactivate-t4a", test_nci_sm_dscvr_poll_deact_t4a },
    { "discovery-poll-activate-error1",  test_nci_sm_dscvr_poll_act_error1 },
    { "discovery-poll-activate-error2",  test_nci_sm_dscvr_poll_act_error2 },
    { "discovery-poll-activate-error3",  test_nci_sm_dscvr_poll_act_error3 },
    { "discovery-poll-activate-error4",  test_nci_sm_dscvr_poll_act_error4 },
    { "discovery-poll-deactivate-t4a-bad-act-param1",
       test_nci_sm_dscvr_poll_deact_t4a_badparam1 },
    { "discovery-poll-deactivate-t4a-bad-act-param2",
       test_nci_sm_dscvr_poll_deact_t4a_badparam2 },
    { "discovery-deact-timeout", test_nci_sm_deact_timeout },
    { "discovery-ntf-t2t", test_nci_sm_discovery_ntf_t2t },
    { "discovery-ntf-broken", test_nci_sm_discovery_ntf_broken },
    { "discovery-ntf-isodep", test_nci_sm_discovery_ntf_isodep },
    { "discovery-ntf-isodep-fail", test_nci_sm_discovery_ntf_isodep_fail },
    { "discovery-ntf-noselect", test_nci_sm_discovery_ntf_noselect },
    { "nfcdep-listen-disappear", test_nci_sm_nfc_dep_listen_disappear },
    { "nfcdep-listen-timeout", test_nci_sm_nfc_dep_listen_timeout },
    { "nfcdep-listen-sleep", test_nci_sm_nfc_dep_listen_sleep },
    { "ce-poll_a_tech_routing", test_nci_sm_iso_dep_ce_tech_routing },
    { "ce-poll_a_prot_routing", test_nci_sm_iso_dep_ce_prot_routing },
    { "config_default", test_nci_config_abf, test_nci_config_ab_data_default },
    { "config_junk", test_nci_config_abf, test_nci_config_ab_data_junk },
    { "config_ab", test_nci_config_ab, test_nci_config_ab_data },
    { "config_ab_x", test_nci_config_ab, test_nci_config_ab_data_x }
};

/*==========================================================================*
 * Common
 *==========================================================================*/

#define TEST_PREFIX "/nci_core/"
#define TEST_(name) TEST_PREFIX name

int main(int argc, char* argv[])
{
    guint i;

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    g_type_init();
    G_GNUC_END_IGNORE_DEPRECATIONS;
    g_test_init(&argc, &argv, NULL);
    g_test_add_func(TEST_("null"), test_null);
    g_test_add_func(TEST_("restart"), test_restart);
    g_test_add_func(TEST_("init_ok"), test_init_ok);
    g_test_add_func(TEST_("init_failed1"), test_init_failed1);
    g_test_add_func(TEST_("init_failed2"), test_init_failed2);
    for (i = 0; i < G_N_ELEMENTS(nci_sm_tests); i++) {
        const TestNciSmData* test = nci_sm_tests + i;
        char* path = g_strconcat(TEST_PREFIX "sm/", test->name, NULL);

        g_test_add_data_func(path, test, test_nci_sm);
        g_free(path);
    }
    test_init(&test_opt, argc, argv);
    return g_test_run();
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
