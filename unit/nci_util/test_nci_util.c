/*
 * Copyright (C) 2019 Jolla Ltd.
 * Copyright (C) 2019 Slava Monich <slava.monich@jolla.com>
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
        .expected = { .poll_b = { {0x65, 0xe6, 0x70, 0x15}, 256 } }
    },{
        .name = "poll_b_rfu", /* RFU part of FSCI to FSC conversion table */
        .mode = NCI_MODE_PASSIVE_POLL_B,
        .data = { TEST_ARRAY_AND_SIZE(mode_param_success_data_poll_b_rfu) },
        .expected = { .poll_b = { {0x65, 0xe6, 0x70, 0x15}, 256 } }
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
static const TestModeParamFailData mode_param_fail_tests[] = {
    {
        .name = "unhandled_mode",
        .mode = NCI_MODE_PASSIVE_LISTEN_15693
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
        .data = { TEST_ARRAY_AND_SIZE(discover_fail_data_too_short_2) }
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
