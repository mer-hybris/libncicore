/*
 * Copyright (C) 2019-2020 Jolla Ltd.
 * Copyright (C) 2019-2020 Slava Monich <slava.monich@jolla.com>
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

#include "nci_util.h"

static TestOpt test_opt;

/*==========================================================================*
 * null
 *==========================================================================*/

static
void
test_null(
    void)
{
    g_assert(!nci_discovery_ntf_copy_array(NULL, 0));
    g_assert(!nci_discovery_ntf_copy(NULL));
}

/*==========================================================================*
 * listen_mode
 *==========================================================================*/

static
void
test_listen_mode(
    void)
{
    /* Listen modes */
    g_assert(nci_listen_mode(NCI_MODE_PASSIVE_LISTEN_A));
    g_assert(nci_listen_mode(NCI_MODE_PASSIVE_LISTEN_B));
    g_assert(nci_listen_mode(NCI_MODE_PASSIVE_LISTEN_F));
    g_assert(nci_listen_mode(NCI_MODE_ACTIVE_LISTEN_A));
    g_assert(nci_listen_mode(NCI_MODE_ACTIVE_LISTEN_F));
    g_assert(nci_listen_mode(NCI_MODE_PASSIVE_LISTEN_15693));

    /* Poll modes */
    g_assert(!nci_listen_mode(NCI_MODE_PASSIVE_POLL_A));
    g_assert(!nci_listen_mode(NCI_MODE_PASSIVE_POLL_B));
    g_assert(!nci_listen_mode(NCI_MODE_PASSIVE_POLL_F));
    g_assert(!nci_listen_mode(NCI_MODE_ACTIVE_POLL_A));
    g_assert(!nci_listen_mode(NCI_MODE_ACTIVE_POLL_F));
    g_assert(!nci_listen_mode(NCI_MODE_PASSIVE_POLL_15693));

    /* Invalid mode */
    g_assert(!nci_listen_mode((NCI_MODE)(-1)));
}

/*==========================================================================*
 * mode_param_ok
 *==========================================================================*/

typedef struct test_mode_param_success_data {
    const char* name;
    NCI_MODE mode;
    GUtilData data;
    NciModeParam expected;
} TestModeParamSuccessData;

static
void
test_mode_param_success(
    gconstpointer user_data)
{
    const TestModeParamSuccessData* test = user_data;
    NciModeParam param;

    memset(&param, 0, sizeof(param));
    g_assert(nci_parse_mode_param(&param, test->mode,
        test->data.bytes, test->data.size));
    g_assert(!memcmp(&param, &test->expected, sizeof(param)));
}


static const guint8 mode_param_success_data_minimal[] =
    { 0x04, 0x00, 0x00, 0x00 };
static const guint8 mode_param_success_data_full[] =
    { 0x04, 0x00, 0x04, 0x37, 0xf4, 0x95, 0x95, 0x01, 0x20 };
static const guint8 mode_param_success_data_no_nfcid1[] =
    { 0x04, 0x00, 0x00, 0x01, 0x20 };
static const guint8 mode_param_success_data_poll_b[] =
    { 0x0b, 0x65, 0xe6, 0x70, 0x15, 0xe1, 0xf3, 0x5e, 0x11, 0x77, 0x87, 0x95 };
static const guint8 mode_param_success_data_poll_b_rfu[] =
    { 0x0b, 0x65, 0xe6, 0x70, 0x15, 0xe1, 0xf3, 0x5e, 0x11, 0x77, 0x97, 0x95 };
static const guint8 mode_param_success_data_poll_f_1[] =
    { 0x01, 0x12, 0x01, 0xfe, 0xc0, 0xf1, 0xc4, 0x41, 0x38, 0x21, 0xc0, 0xc1,
      0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0x0f, 0xab };
static const guint8 mode_param_success_data_poll_f_2[] =
    { 0x02, 0x12, 0x01, 0xfe, 0xc0, 0xf1, 0xc4, 0x41, 0x38, 0x21, 0xc0, 0xc1,
      0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0x0f, 0xab };
static const guint8 mode_param_success_data_poll_f_3[] =
    { 0x03, 0x12, 0x01, 0xfe, 0xc0, 0xf1, 0xc4, 0x41, 0x38, 0x21, 0xc0, 0xc1,
      0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0x0f, 0xab };
static const guint8 mode_param_success_data_listen_f_0[] =
    { 0x00 };
static const guint8 mode_param_success_data_listen_f_1[] =
    { 0x08, 0x01, 0xfe, 0xc0, 0xf1, 0xc4, 0x41, 0x38, 0x21 };
static const guint8 mode_param_success_data_listen_f_2[] =
    { 0x00, /* rest is ignored */ 0x01, 0xfe, 0xc0, 0xf1 };
static const TestModeParamSuccessData mode_param_success_tests[] = {
    {
        .name = "minimal",
        .mode = NCI_MODE_ACTIVE_POLL_A,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_success_data_minimal) },
        .expected = { .poll_a = { { 0x04, 0x00 } } }
    },{
        .name = "no_nfcid1",
        .mode = NCI_MODE_ACTIVE_POLL_A,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_success_data_no_nfcid1) },
        .expected = { .poll_a = { { 0x04, 0x00 }, 0, { 0 }, 1, 0x20 } }
    },{
        .name = "full",
        .mode = NCI_MODE_PASSIVE_POLL_A,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_success_data_full) },
        .expected = {
            .poll_a = { {0x04, 0x00}, 4, {0x37, 0xf4, 0x95, 0x95}, 1, 0x20 }
        }
    },{
        .name = "poll_b",
        .mode = NCI_MODE_PASSIVE_POLL_B,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_success_data_poll_b) },
        .expected = { .poll_b = { {0x65, 0xe6, 0x70, 0x15}, 256,
                                  {0xe1, 0xf3, 0x5e, 0x11},
                                  {mode_param_success_data_poll_b + 9, 3}}}
    },{
        .name = "poll_b_rfu", /* RFU part of FSCI to FSC conversion table */
        .mode = NCI_MODE_PASSIVE_POLL_B,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_success_data_poll_b_rfu) },
        .expected = { .poll_b = { {0x65, 0xe6, 0x70, 0x15}, 256,
                                  {0xe1, 0xf3, 0x5e, 0x11},
                                  {mode_param_success_data_poll_b_rfu + 9, 3}}}
    },{
        .name = "active_poll_f",
        .mode = NCI_MODE_ACTIVE_POLL_F,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_success_data_poll_f_1) },
        .expected = { .poll_f = { 1, {0x01, 0xfe, 0xc0, 0xf1,
                                      0xc4, 0x41, 0x38, 0x21} } }
    },{
        .name = "passive_poll_f",
        .mode = NCI_MODE_PASSIVE_POLL_F,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_success_data_poll_f_2) },
        .expected = { .poll_f = { 2, {0x01, 0xfe, 0xc0, 0xf1,
                                      0xc4, 0x41, 0x38, 0x21} } }
    },{
        .name = "passive_poll_f_3",
        .mode = NCI_MODE_PASSIVE_POLL_F,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_success_data_poll_f_3) },
        .expected = { .poll_f = { 3, {0x01, 0xfe, 0xc0, 0xf1,
                                      0xc4, 0x41, 0x38, 0x21} } }
    },{
        .name = "active_listen_f",
        .mode = NCI_MODE_ACTIVE_LISTEN_F,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_success_data_listen_f_0) },
    },{
        .name = "passive_listen_f",
        .mode = NCI_MODE_PASSIVE_LISTEN_F,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_success_data_listen_f_1) },
        .expected = { .listen_f = {{mode_param_success_data_listen_f_1+1, 8}}}
    },{
        .name = "passive_listen_f_2",
        .mode = NCI_MODE_PASSIVE_LISTEN_F,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_success_data_listen_f_2) }
    }
};

/*==========================================================================*
 * mode_param_fail
 *==========================================================================*/

typedef struct test_mode_param_fail_data {
    const char* name;
    NCI_MODE mode;
    GUtilData data;
} TestModeParamFailData;

static
void
test_mode_param_fail(
    gconstpointer user_data)
{
    const TestModeParamFailData* test = user_data;
    NciModeParam param;

    g_assert(!nci_parse_mode_param(&param, test->mode,
        test->data.bytes, test->data.size));
}

static const guint8 mode_param_fail_data_too_short_1[] =
    { 0x00 };
static const guint8 mode_param_fail_data_too_short_2[] =
    { 0x04, 0x00, 0x04, 0x37, 0xf4 };
static const guint8 mode_param_fail_data_too_short_3[] =
    { 0x04, 0x00, 0x04, 0x37, 0xf4, 0x95, 0x95, 0x01 };
static const guint8 mode_param_fail_data_too_long[] =
    { 0x04, 0x00, 0x0b /* exceeds max 10 */, 0x01, 0x02, 0x03,
      0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x01, 0x20 };
static const guint8 mode_param_fail_pollb_data_too_short_1[] =
    { 0x0a, 0x65, 0xe6, 0x70, 0x15, 0xe1, 0xf3, 0x5e, 0x11, 0x77, 0x87 };
static const guint8 mode_param_fail_pollf_data_too_short_1[] =
    { 0x01, 0x12, 0x01, 0xfe, 0xc0, 0xf1, 0xc4, 0x41, 0x38, 0x21, 0xc0, 0xc1,
      0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0x0f };
static const guint8 mode_param_fail_pollf_data_too_short_2[] =
    { 0x01, 0x07, 0x01, 0xfe, 0xc0, 0xf1, 0xc4, 0x41, 0x38 };
static const guint8 mode_param_fail_listenf_data_too_short[] =
    { 0x08, 0x01, 0xfe, 0xc0, 0xf1, 0xc4, 0x41, 0x38 };
static const guint8 mode_param_fail_listenf_data_bad_len[] =
    { 0x09, 0x01, 0xfe, 0xc0, 0xf1, 0xc4, 0x41, 0x38, 0x21, 0x00 };
static const TestModeParamFailData mode_param_fail_tests[] = {
    {
        .name = "invalid_mode",
        .mode = (NCI_MODE)(-1)
    },{
        .name = "unhandled_mode",
        .mode = NCI_MODE_PASSIVE_LISTEN_15693
    },{
        .name = "listen_mode",
        .mode = NCI_MODE_PASSIVE_LISTEN_A
    },{
        .name = "passive_poll_a_empty",
        .mode = NCI_MODE_PASSIVE_POLL_A,
        .data = { NULL, 0 }
    },{
        .name = "active_poll_a_empty",
        .mode = NCI_MODE_ACTIVE_POLL_A,
        .data = { NULL, 0 }
    },{
        .name = "too_short/poll_a",
        .mode = NCI_MODE_ACTIVE_POLL_A,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_fail_data_too_short_1) }
    },{
        .name = "too_short/poll_b",
        .mode = NCI_MODE_ACTIVE_POLL_A,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_fail_data_too_short_1) }
    },{
        .name = "too_short/poll_f",
        .mode = NCI_MODE_ACTIVE_POLL_F,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_fail_data_too_short_1) }
    },{
        .name = "too_short/2",
        .mode = NCI_MODE_ACTIVE_POLL_A,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_fail_data_too_short_2) }
    },{
        .name = "too_short/3",
        .mode = NCI_MODE_ACTIVE_POLL_A,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_fail_data_too_short_3) }
    },{
        .name = "too_long",
        .mode = NCI_MODE_ACTIVE_POLL_A,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_fail_data_too_long) }
    },{
        .name = "poll_b_empty",
        .mode = NCI_MODE_PASSIVE_POLL_B,
        .data = { NULL, 0 }
    },{
        .name = "poll_b_too_short",
        .mode = NCI_MODE_PASSIVE_POLL_B,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_fail_pollb_data_too_short_1) }
    },{
        .name = "poll_f_too_short_1",
        .mode = NCI_MODE_PASSIVE_POLL_F,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_fail_pollf_data_too_short_1) }
    },{
        .name = "poll_f_too_short_2",
        .mode = NCI_MODE_PASSIVE_POLL_F,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_fail_pollf_data_too_short_2) }
    },{
        .name = "listen_f_empty",
        .mode = NCI_MODE_ACTIVE_LISTEN_F,
    },{
        .name = "listen_f_too_short",
        .mode = NCI_MODE_ACTIVE_LISTEN_F,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_fail_listenf_data_too_short) }
    },{
        .name = "listen_f_bad_len",
        .mode = NCI_MODE_PASSIVE_LISTEN_F,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_fail_listenf_data_bad_len) }
    }
};

/*==========================================================================*
 * intf_activated_success
 *==========================================================================*/

typedef struct test_intf_activated_success_data {
    const char* name;
    GUtilData data;
    const NciModeParam* mode_param;
    const NciActivationParam* activation_param;
} TestIntfActivatedSuccessData;

static
void
test_intf_activated_success(
    gconstpointer user_data)
{
    const TestIntfActivatedSuccessData* test = user_data;
    NciIntfActivationNtf ntf;
    NciModeParam mode_param;
    NciActivationParam activation_param;

    memset(&mode_param, 0, sizeof(mode_param));
    memset(&activation_param, 0, sizeof(activation_param));
    g_assert(nci_parse_intf_activated_ntf(&ntf, &mode_param, &activation_param,
        test->data.bytes, test->data.size));
    g_assert(!ntf.mode_param == !test->mode_param);
    g_assert(!ntf.mode_param || !memcmp(ntf.mode_param,
        test->mode_param, sizeof(mode_param)));
    if (test->activation_param) {
        g_assert(ntf.activation_param);
        g_assert(!memcmp(ntf.activation_param, test->activation_param,
             sizeof(activation_param)));
    } else {
        g_assert(!ntf.activation_param);
    }
}

static const guint8 test_intf_activated_ntf_mifare[] = {
    0x01, 0x80, 0x80, 0x00, 0xff, 0x01, 0x0c, 0x44,
    0x00, 0x07, 0x04, 0x47, 0x8a, 0x92, 0x7f, 0x51,
    0x80, 0x01, 0x08, 0x00, 0x00, 0x00, 0x00
};
static const guint8 test_intf_activated_ntf_nfc_dep_poll_1[] = {
    0x01, 0x03, 0x05, 0x00, 0xfb, 0x01, 0x09, 0x08,
    0x00, 0x04, 0x08, 0x50, 0xad, 0x0e, 0x01, 0x40,
    0x00, 0x02, 0x02, 0x21, 0x20, 0xc2, 0x40, 0x83,
    0x1b, 0xe1, 0x22, 0x5d, 0xfe, 0xb7, 0xe9, 0x00,
    0x00, 0x00, 0x0e, 0x32, 0x46, 0x66, 0x6d, 0x01,
    0x01, 0x11, 0x02, 0x02, 0x07, 0xff, 0x03, 0x02,
    0x00, 0x03, 0x04, 0x01, 0xff
};
static const guint8 test_intf_activated_ntf_nfc_dep_poll_2[] = {
    0x01, 0x03, 0x05, 0x03, 0xfb, 0x01, 0x00, 0x03,
    0x02, 0x02, 0x21, 0x20, 0xc2, 0x40, 0x83, 0x1b,
    0xe1, 0x22, 0x5d, 0xfe, 0xb7, 0xe9, 0x00, 0x00,
    0x00, 0x0e, 0x32, 0x46, 0x66, 0x6d, 0x01, 0x01,
    0x11, 0x02, 0x02, 0x07, 0xff, 0x03, 0x02, 0x00,
    0x03, 0x04, 0x01, 0xff
};
static const guint8 test_intf_activated_ntf_nfc_dep_poll_3[] = {
    0x01, 0x03, 0x05, 0x00, 0xfb, 0x01, 0x09, 0x08,
    0x00, 0x04, 0x08, 0x50, 0xad, 0x0e, 0x01, 0x40,
    0x02, 0x02, 0x02, 0x10, 0x0f, 0xc2, 0x40, 0x83,
    0x1b, 0xe1, 0x22, 0x5d, 0xfe, 0xb7, 0xe9, 0x00,
    0x00, 0x00, 0x0e, 0x32
};
static const guint8 test_intf_activated_ntf_nfc_dep_listen_1[] = {
    0x01, 0x03, 0x05, 0x83, 0xfb, 0x01, 0x00, 0x83,
    0x00, 0x00, 0x20, 0x1f, 0xc5, 0x47, 0xe4, 0x98,
    0x4d, 0x88, 0x04, 0xb4, 0x92, 0xe5, 0x00, 0x00,
    0x00, 0x32, 0x46, 0x66, 0x6d, 0x01, 0x01, 0x11,
    0x02, 0x02, 0x07, 0xff, 0x03, 0x02, 0x00, 0x13,
    0x04, 0x01, 0xff
};

static const guint8 test_intf_activated_ntf_nfc_dep_listen_2[] = {
    0x01, 0x03, 0x05, 0x83, 0xfb, 0x01, 0x00, 0x83,
    0x00, 0x00, 0x0f, 0x0e, 0xc5, 0x47, 0xe4, 0x98,
    0x4d, 0x88, 0x04, 0xb4, 0x92, 0xe5, 0x00, 0x00,
    0x00, 0x32
};

static const NciModeParam test_intf_activated_ntf_mifare_mp = {
    .poll_a = {
        .sens_res = { 0x44, 0x00 },
        .nfcid1_len = 7,
        .nfcid1 = { 0x04, 0x47, 0x8a, 0x92, 0x7f, 0x51, 0x80 },
        .sel_res_len = 1,
        .sel_res = 0x08
    }
};
static const NciModeParam test_intf_activated_ntf_poll_a_mp = {
    .poll_a = {
        .sens_res = { 0x08, 0x00 },
        .nfcid1_len = 4,
        .nfcid1 = { 0x08, 0x50, 0xad, 0x0e },
        .sel_res_len = 1,
        .sel_res = 0x40
    }
};
static const NciActivationParam test_intf_activated_ntf_nfc_dep_poll_ap_1 = {
    .nfc_dep_poll = {
        .nfcid3 = { 0xc2, 0x40, 0x83, 0x1b, 0xe1,
                    0x22, 0x5d, 0xfe, 0xb7, 0xe9 },
        .did = 0x00,
        .bs = 0x00,
        .br = 0x00,
        .to = 0x0e,
        .pp = 0x32,
        .g = { test_intf_activated_ntf_nfc_dep_poll_1 + 0x24, 17 },
    }
};
static const NciActivationParam test_intf_activated_ntf_nfc_dep_poll_ap_2 = {
    .nfc_dep_poll = {
        .nfcid3 = { 0xc2, 0x40, 0x83, 0x1b, 0xe1,
                    0x22, 0x5d, 0xfe, 0xb7, 0xe9 },
        .did = 0x00,
        .bs = 0x00,
        .br = 0x00,
        .to = 0x0e,
        .pp = 0x32,
        .g = { test_intf_activated_ntf_nfc_dep_poll_2 + 0x1b, 17 },
    }
};
static const NciActivationParam test_intf_activated_ntf_nfc_dep_poll_ap_3 = {
    .nfc_dep_poll = {
        .nfcid3 = { 0xc2, 0x40, 0x83, 0x1b, 0xe1,
                    0x22, 0x5d, 0xfe, 0xb7, 0xe9 },
        .did = 0x00,
        .bs = 0x00,
        .br = 0x00,
        .to = 0x0e,
        .pp = 0x32,
    }
};
static const NciActivationParam test_intf_activated_ntf_nfc_dep_listen_ap_1 = {
    .nfc_dep_listen = {
        .nfcid3 = { 0xc5, 0x47, 0xe4, 0x98, 0x4d,
                    0x88, 0x04, 0xb4, 0x92, 0xe5 },
        .did = 0x00,
        .bs = 0x00,
        .br = 0x00,
        .pp = 0x32,
        .g = { test_intf_activated_ntf_nfc_dep_listen_1 + 0x1a, 17 },
    }
};
static const NciActivationParam test_intf_activated_ntf_nfc_dep_listen_ap_2 = {
    .nfc_dep_listen = {
        .nfcid3 = { 0xc5, 0x47, 0xe4, 0x98, 0x4d,
                    0x88, 0x04, 0xb4, 0x92, 0xe5 },
        .did = 0x00,
        .bs = 0x00,
        .br = 0x00,
        .pp = 0x32
    }
};

static const TestIntfActivatedSuccessData intf_activated_success_tests[] = {
    {
        .name = "mifare/ok",
        .data = {TEST_ARRAY_AND_SIZE(test_intf_activated_ntf_mifare)},
        .mode_param = &test_intf_activated_ntf_mifare_mp,
        .activation_param = NULL
    },{
        .name = "nfc_dep_poll/ok/1",
        .data = {TEST_ARRAY_AND_SIZE(test_intf_activated_ntf_nfc_dep_poll_1)},
        .mode_param = &test_intf_activated_ntf_poll_a_mp,
        .activation_param = &test_intf_activated_ntf_nfc_dep_poll_ap_1
    },{
        .name = "nfc_dep_poll/ok/2",
        .data = {TEST_ARRAY_AND_SIZE(test_intf_activated_ntf_nfc_dep_poll_2)},
        .activation_param = &test_intf_activated_ntf_nfc_dep_poll_ap_2
    },{
        .name = "nfc_dep_poll/ok/3",
        .data = {TEST_ARRAY_AND_SIZE(test_intf_activated_ntf_nfc_dep_poll_3)},
        .mode_param = &test_intf_activated_ntf_poll_a_mp,
        .activation_param = &test_intf_activated_ntf_nfc_dep_poll_ap_3
    },{
        .name = "nfc_dep_listen/ok/1",
        .data = {TEST_ARRAY_AND_SIZE(test_intf_activated_ntf_nfc_dep_listen_1)},
        .activation_param = &test_intf_activated_ntf_nfc_dep_listen_ap_1
    },{
        .name = "nfc_dep_listen/ok/2",
        .data = {TEST_ARRAY_AND_SIZE(test_intf_activated_ntf_nfc_dep_listen_2)},
        .activation_param = &test_intf_activated_ntf_nfc_dep_listen_ap_2
    }
};

/*==========================================================================*
 * intf_activated_fail
 *==========================================================================*/

typedef struct test_intf_activated_fail_data {
    const char* name;
    GUtilData data;
    gboolean parse_ok;
    gboolean mode_param_ok;
    gboolean activation_param_ok;
} TestIntfActivatedFailData;

static
void
test_intf_activated_fail(
    gconstpointer user_data)
{
    const TestIntfActivatedFailData* test = user_data;
    NciIntfActivationNtf ntf;
    NciModeParam mode_param;
    NciActivationParam activation_param;

    g_assert(nci_parse_intf_activated_ntf(&ntf, &mode_param, &activation_param,
        test->data.bytes, test->data.size) == test->parse_ok);
    g_assert(!ntf.mode_param == !test->mode_param_ok);
    g_assert(!ntf.activation_param == !test->activation_param_ok);
}

static const guint8 test_intf_activated_ntf_nfc_dep_fail_1[] = {
    0x01, 0x03, 0x05, 0x00, 0xfb, 0x01, 0x09, 0x08,
    0x00, 0x04, 0x08, 0x50, 0xad, 0x0e, 0x01, 0x40,
    0x02, 0x02, 0x02, 0x21, 0x21, 0xc2, 0x40, 0x83,
                           /* ^^ too large */
    0x1b, 0xe1, 0x22, 0x5d, 0xfe, 0xb7, 0xe9, 0x00,
    0x00, 0x00, 0x0e, 0x32, 0x46, 0x66, 0x6d, 0x01,
    0x01, 0x11, 0x02, 0x02, 0x07, 0xff, 0x03, 0x02,
    0x00, 0x03, 0x04, 0x01, 0xff
};
static const guint8 test_intf_activated_ntf_nfc_dep_fail_2[] = {
    0x01, 0x03, 0x05, 0x00, 0xfb, 0x01, 0x09, 0x08,
    0x00, 0x04, 0x08, 0x50, 0xad, 0x0e, 0x01, 0x40,
    0x02, 0x02, 0x02, 0x0c, 0x0b, 0xc2, 0x40, 0x83,
                           /* ^^ too short */
    0x1b, 0xe1, 0x22, 0x5d, 0xfe, 0xb7, 0xe9, 0x00
};
static const guint8 test_intf_activated_ntf_nfc_dep_fail_3[] = {
    0x01, 0x03, 0x05, 0x83, 0xfb, 0x01, 0x00, 0x83,
    0x00, 0x00, 0x20, 0x20, 0xc5, 0x47, 0xe4, 0x98,
    /* ATR_REQ too long ^^ */
    0x4d, 0x88, 0x04, 0xb4, 0x92, 0xe5, 0x00, 0x00,
    0x00, 0x32, 0x46, 0x66, 0x6d, 0x01, 0x01, 0x11,
    0x02, 0x02, 0x07, 0xff, 0x03, 0x02, 0x00, 0x13,
    0x04, 0x01, 0xff
};

static const guint8 test_intf_activated_ntf_nfc_dep_fail_4[] = {
    0x01, 0x03, 0x05, 0x83, 0xfb, 0x01, 0x00, 0x83,
    0x00, 0x00, 0x05, 0x04, 0xc5, 0x47, 0xe4, 0x98
               /* ^^    ^^ ATR_REQ too short */
};

static const TestIntfActivatedFailData intf_activated_fail_tests[] = {
    {
        .name = "nfc_dep_1",
        .data = {TEST_ARRAY_AND_SIZE(test_intf_activated_ntf_nfc_dep_fail_1)},
        .parse_ok = TRUE, .mode_param_ok = TRUE, .activation_param_ok = FALSE
    },{
        .name = "nfc_dep_2",
        .data = {TEST_ARRAY_AND_SIZE(test_intf_activated_ntf_nfc_dep_fail_2)},
        .parse_ok = TRUE, .mode_param_ok = TRUE, .activation_param_ok = FALSE
    },{
        .name = "nfc_dep_3",
        .data = {TEST_ARRAY_AND_SIZE(test_intf_activated_ntf_nfc_dep_fail_3)},
        .parse_ok = TRUE, .mode_param_ok = FALSE, .activation_param_ok = FALSE
    },{
        .name = "nfc_dep_4",
        .data = {TEST_ARRAY_AND_SIZE(test_intf_activated_ntf_nfc_dep_fail_4)},
        .parse_ok = TRUE, .mode_param_ok = FALSE, .activation_param_ok = FALSE
    }
};

/*==========================================================================*
 * discover_success
 *==========================================================================*/

typedef struct test_discover_success_data {
    const char* name;
    GUtilData data;
    NciDiscoveryNtf ntf;
    
} TestDiscoverSuccessData;

static
void
test_discover_success(
    gconstpointer test_data)
{
    const TestDiscoverSuccessData* test = test_data;
    const GUtilData* pkt = &test->data;
    NciDiscoveryNtf ntf;
    NciModeParam param;

    g_assert(nci_parse_discover_ntf(&ntf, NULL, pkt->bytes, pkt->size));
    g_assert(nci_parse_discover_ntf(&ntf, &param, pkt->bytes, pkt->size));
}

static const guint8 discover_success_data_no_param[] =
    { 0x01, 0x04, 0x00, 0x00, 0x02 };
static const NciModeParam discover_success_data_full_1_param = {
    .poll_a = { {0x04, 0x00}, 4, { 0x4f, 0x01, 0x74, 0x01 }, 1, 0x20 }
};
static const guint8 discover_success_data_full_1[] =
    { 0x01, 0x04, 0x00, 0x09, 0x04, 0x00, 0x04, 0x4f,
      0x01, 0x74, 0x01, 0x01, 0x20, 0x02 };
static const NciModeParam discover_success_data_full_2_param = {
    .poll_a = { {0x04, 0x00}, 4, { 0x4f, 0x01, 0x74, 0x01 }, 1, 0x08 }
};
static const guint8 discover_success_data_full_2[] =
    { 0x02, 0x80, 0x00, 0x09, 0x04, 0x00, 0x04, 0x4f,
      0x01, 0x74, 0x01, 0x01, 0x08, 0x00 };

static const TestDiscoverSuccessData discover_success_tests[] = {
    {
        .name = "no_param",
        .data = { TEST_ARRAY_AND_SIZE(discover_success_data_no_param) },
        .ntf = { .discovery_id = 0x01, .protocol = NCI_PROTOCOL_ISO_DEP,
                 .mode = NCI_MODE_PASSIVE_POLL_A, .param_len = 0,
                 .last = FALSE }
    },{
        .name = "full/1",
        .data = { TEST_ARRAY_AND_SIZE(discover_success_data_full_1) },
        .ntf = { .discovery_id = 0x01, .protocol = NCI_PROTOCOL_ISO_DEP,
                 .mode = NCI_MODE_PASSIVE_POLL_A, .param_len = 9,
                 .param_bytes = discover_success_data_full_1 + 4,
                 .param = &discover_success_data_full_1_param,
                 .last = FALSE }
    },{
        .name = "full/2",
        .data = { TEST_ARRAY_AND_SIZE(discover_success_data_full_2) },
        .ntf = { .discovery_id = 0x01, .protocol = 0x80,
                 .mode = NCI_MODE_PASSIVE_POLL_A, .param_len = 9,
                 .param_bytes = discover_success_data_full_2 + 4,
                 .param = &discover_success_data_full_2_param,
                 .last = TRUE }
    }
};

/*==========================================================================*
 * discover_copy
 *==========================================================================*/

static
void
test_discover_copy_check(
    const NciDiscoveryNtf* n1,
    const NciDiscoveryNtf* n2)
{
    g_assert(n1);
    g_assert(n1->discovery_id == n2->discovery_id);
    g_assert(n1->protocol == n2->protocol);
    g_assert(n1->mode == n2->mode);
    g_assert(n1->param_len == n2->param_len);
    g_assert(n1->last == n2->last);

    if (n2->param_bytes) {
        g_assert(n1->param_bytes);
        g_assert(!memcmp(n1->param_bytes, n2->param_bytes, n2->param_len));
    } else {
        g_assert(!n1->param_bytes);
    }
    
    if (n2->param) {
        g_assert(n1->param);
        g_assert(!memcmp(n1->param_bytes, n2->param_bytes, n2->param_len));
        switch (n2->mode) {
        case NCI_MODE_ACTIVE_POLL_A:
        case NCI_MODE_PASSIVE_POLL_A:
            {
                const NciModeParamPollA* p1 = &n1->param->poll_a;
                const NciModeParamPollA* p2 = &n2->param->poll_a;

                g_assert(p1->sens_res[0] == p2->sens_res[0]);
                g_assert(p1->sens_res[1] == p2->sens_res[1]);
                g_assert(p1->nfcid1_len == p2->nfcid1_len);
                g_assert(p1->sel_res_len == p2->sel_res_len);
                g_assert(p1->sel_res == p2->sel_res);
                g_assert(!memcmp(p1->nfcid1, p2->nfcid1, p2->nfcid1_len));
            }
            break;
        default:
            break;
        }
    } else {
        g_assert(!n1->param);
    }
}

static
void
test_discover_copy(
    gconstpointer test_data)
{
    const TestDiscoverSuccessData* test = test_data;
    NciDiscoveryNtf ntf = test->ntf;
    NciDiscoveryNtf* copy = nci_discovery_ntf_copy(&test->ntf);

    test_discover_copy_check(&test->ntf, copy);
    g_free(copy);

    ntf.param = NULL;
    copy = nci_discovery_ntf_copy(&ntf);
    test_discover_copy_check(&ntf, copy);
    g_free(copy);
}

/*==========================================================================*
 * discover_fail
 *==========================================================================*/

typedef struct test_discover_fail_data {
    const char* name;
    GUtilData data;
} TestDiscoverFailData;

static
void
test_discover_fail(
    gconstpointer test_data)
{
    const TestDiscoverFailData* test = test_data;
    const GUtilData* pkt = &test->data;
    NciDiscoveryNtf ntf;
    NciModeParam param;

    g_assert(!nci_parse_discover_ntf(&ntf, &param, pkt->bytes, pkt->size));
}

static const guint8 discover_fail_data_too_short_1[] =
    {  0x01, 0x04, 0x00, 0x09 };
static const guint8 discover_fail_data_too_short_2[] =
    {  0x01, 0x04, 0x00, 0x09, 0x04, 0x00, 0x04, 0x4f };
static const guint8 discover_fail_data_too_short_3[] =
    {  0x01, 0x04, 0x00, 0x09, 0x04, 0x00, 0x04, 0x4f,
       0x01, 0x74, 0x01, 0x01, 0x20 };
static const TestDiscoverFailData discover_fail_tests[] = {
    {
        .name = "too_short/1",
        .data = { TEST_ARRAY_AND_SIZE(discover_fail_data_too_short_1) }
    },{
        .name = "too_short/2",
        .data = { TEST_ARRAY_AND_SIZE(discover_fail_data_too_short_2) }
    },{
        .name = "too_short/3",
        .data = { TEST_ARRAY_AND_SIZE(discover_fail_data_too_short_3) }
    }
};

/*==========================================================================*
 * Common
 *==========================================================================*/

#define TEST_(name) "/nci_util/" name

int main(int argc, char* argv[])
{
    guint i;

    g_test_init(&argc, &argv, NULL);
    test_init(&test_opt, argc, argv);
    g_test_add_func(TEST_("null"), test_null);
    g_test_add_func(TEST_("listen_mode"), test_listen_mode);
    for (i = 0; i < G_N_ELEMENTS(mode_param_success_tests); i++) {
        const TestModeParamSuccessData* test = mode_param_success_tests + i;
        char* path = g_strconcat(TEST_("mode_param/ok/"), test->name, NULL);

        g_test_add_data_func(path, test, test_mode_param_success);
        g_free(path);
    }
    for (i = 0; i < G_N_ELEMENTS(mode_param_fail_tests); i++) {
        const TestModeParamFailData* test = mode_param_fail_tests + i;
        char* path = g_strconcat(TEST_("mode_param/fail/"), test->name, NULL);

        g_test_add_data_func(path, test, test_mode_param_fail);
        g_free(path);
    }
    for (i = 0; i < G_N_ELEMENTS(discover_success_tests); i++) {
        const TestDiscoverSuccessData* test = discover_success_tests + i;
        char* path1 = g_strconcat(TEST_("discover/success/"), test->name, NULL);
        char* path2 = g_strconcat(TEST_("discover/copy/"), test->name, NULL);

        g_test_add_data_func(path1, test, test_discover_success);
        g_test_add_data_func(path2, test, test_discover_copy);
        g_free(path1);
        g_free(path2);
    }
    for (i = 0; i < G_N_ELEMENTS(intf_activated_success_tests); i++) {
        const TestIntfActivatedSuccessData* test =
            intf_activated_success_tests + i;
        char* path = g_strconcat(TEST_("intf_activated/success/"),
            test->name, NULL);

        g_test_add_data_func(path, test, test_intf_activated_success);
        g_free(path);
    }
    for (i = 0; i < G_N_ELEMENTS(intf_activated_fail_tests); i++) {
        const TestIntfActivatedFailData* test = intf_activated_fail_tests + i;
        char* path = g_strconcat(TEST_("intf_activated/fail/"),
            test->name, NULL);

        g_test_add_data_func(path, test, test_intf_activated_fail);
        g_free(path);
    }
    for (i = 0; i < G_N_ELEMENTS(discover_fail_tests); i++) {
        const TestDiscoverFailData* test = discover_fail_tests + i;
        char* path = g_strconcat(TEST_("discover/fail/"), test->name, NULL);

        g_test_add_data_func(path, test, test_discover_fail);
        g_free(path);
    }
    return g_test_run();
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
