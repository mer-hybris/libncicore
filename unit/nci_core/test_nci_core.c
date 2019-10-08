/*
 * Copyright (C) 2018-2019 Jolla Ltd.
 * Copyright (C) 2018-2019 Slava Monich <slava.monich@jolla.com>
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
#include <gutil_log.h>

static TestOpt test_opt;

#define TEST_TIMEOUT (20) /* seconds */
#define TEST_DEFAULT_CMD_TIMEOUT (10000) /* milliseconds */

static const guint8 CORE_RESET_RSP[] = {
    0x40, 0x00, 0x03, 0x00, 0x10, 0x00
};
static const guint8 CORE_RESET_RSP_ERROR[] = {
    0x40, 0x00, 0x03, 0x03, 0x10, 0x00
};
static const guint8 CORE_RESET_V2_RSP[] = {
    0x40, 0x00, 0x01, 0x00
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
static const guint8 CORE_GET_CONFIG_RSP[] = {
    0x40, 0x03, 0x0b, 0x00, 0x03, 0x08, 0x01, 0x00,
    0x11, 0x01, 0x00, 0x22, 0x01, 0x00
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
static const guint8 CORE_SET_CONFIG_RSP[] = {
    0x40, 0x02, 0x02, 0x00, 0x00
};
static const guint8 CORE_SET_CONFIG_RSP_ERROR[] = {
    0x40, 0x02, 0x02, NCI_STATUS_REJECTED, 0x00
};
static const guint8 CORE_SET_CONFIG_RSP_INVALID_PARAM[] = {
    0x40, 0x02, 0x03, NCI_STATUS_INVALID_PARAM, 0x01, 0x11
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
static const guint8 RF_DISCOVER_MAP_RSP[] = {
    0x41, 0x00, 0x01, 0x00
};
static const guint8 RF_DISCOVER_MAP_ERROR[] = {
    0x41, 0x00, 0x01, 0x03
};
static const guint8 RF_DISCOVER_MAP_BROKEN[] = {
    0x41, 0x00, 0x00
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
static const guint8 RF_INTF_ACTIVATED_NTF_ISO_DEP[] = {
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
static const guint8 RF_DEACTIVATE_NTF_BROKEN[] = {
    0x61, 0x06, 0x00
};
static const guint8 CORE_GENERIC_ERROR_NTF[] = {
    0x60, 0x07, 0x01, NCI_STATUS_FAILED
};
static const guint8 CORE_GENERIC_TARGET_ACTIVATION_FAILED_ERROR_NTF[] = {
    0x60, 0x07, 0x01, NCI_DISCOVERY_TARGET_ACTIVATION_FAILED
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
static const guint8 RF_DISCOVER_NTF_1_ISO_DEP[] = {
    0x61, 0x03, 0x0e, 0x01, 0x04, 0x00, 0x09, 0x04,
    0x00, 0x04, 0x4f, 0x01, 0x74, 0x01, 0x01, 0x20,
    0x02
};
static const guint8 RF_DISCOVER_NTF_2_T2T[] = {
    0x61, 0x03, 0x0e, 0x02, 0x02, 0x00, 0x09, 0x04,
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
static const guint8 RF_DISCOVER_SELECT_RSP[] = {
    0x41, 0x04, 0x01, 0x00
};

static
gboolean
test_timeout(
    gpointer loop)
{
    g_assert(!"TIMEOUT");
    return G_SOURCE_REMOVE;
}

static
guint
test_setup_timeout(
    GMainLoop* loop)
{
    if (!(test_opt.flags & TEST_FLAG_DEBUG)) {
        return g_timeout_add_seconds(TEST_TIMEOUT, test_timeout, loop);
    } else {
        return 0;
    }
}

static
void
test_bytes_unref(
    gpointer bytes)
{
    g_bytes_unref(bytes);
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
    GPtrArray* written;
    guint write_id;
    guint read_id;
    NciHalClient* sar;
    void* test_data;
    guint fail_write;
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
    if (test_hal_io_next_rsp(hal)) {
        test_hal_io_read_one(hal);
        test_hal_io_flush_ntf(hal);
    }
    return G_SOURCE_REMOVE;
}

static
gboolean
test_hal_io_write_cb(
    gpointer user_data)
{
    TestHalIoWrite* write = user_data;
    TestHalIo* hal = write->hal;

    g_assert(hal->write_id);
    hal->write_id = 0;
    g_ptr_array_add(hal->written, g_bytes_ref(write->bytes));
    if (write->complete) {
        write->complete(hal->sar, TRUE);
    }
    if (!hal->read_id) {
        hal->read_id = g_idle_add(test_hal_io_read_cb, hal);
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
    hal->written = g_ptr_array_new();
    g_ptr_array_set_free_func(hal->written, test_bytes_unref);
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
    g_ptr_array_free(hal->written, TRUE);
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
    guint timeout_id = test_setup_timeout(loop);
    gulong id;

    nci->cmd_timeout = (test_opt.flags & TEST_FLAG_DEBUG) ? 0 :
        TEST_DEFAULT_CMD_TIMEOUT;

    /* Responses */
    test_hal_io_queue_rsp(hal, CORE_RESET_RSP);
    test_hal_io_queue_ntf(hal, CORE_IGNORED_NTF);
    test_hal_io_queue_rsp(hal, CORE_INIT_RSP);
    test_hal_io_queue_rsp(hal, CORE_GET_CONFIG_RSP);
    test_hal_io_queue_rsp(hal, CORE_SET_CONFIG_RSP);

    id = nci_core_add_current_state_changed_handler(nci,
        test_restart_done, loop);

    nci_core_restart(nci);
    g_main_loop_run(loop);
    if (timeout_id) {
        g_source_remove(timeout_id);
    }

    g_assert(nci->current_state == NCI_RFST_IDLE);
    g_assert(nci->next_state == NCI_RFST_IDLE);
    nci_core_remove_handler(nci, id);

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
    guint timeout_id;
    gulong id;

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

    id = nci_core_add_current_state_changed_handler(nci,
        test_init_ok_done, loop);

    timeout_id = test_setup_timeout(loop);
    g_main_loop_run(loop);
    if (timeout_id) {
        g_source_remove(timeout_id);
    }

    g_assert(nci->current_state == NCI_RFST_IDLE);
    g_assert(nci->next_state == NCI_RFST_IDLE);
    nci_core_remove_handlers(nci, &id, 1);
    g_assert(!id);
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
    guint timeout_id;
    gulong id;

    nci_core_set_state(nci, NCI_RFST_IDLE);
    id = nci_core_add_current_state_changed_handler(nci,
        test_init_failed1_done, loop);

    timeout_id = test_setup_timeout(loop);
    g_main_loop_run(loop);
    if (timeout_id) {
        g_source_remove(timeout_id);
    }

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
    guint timeout_id;
    gulong id;

    hal->fail_write++;
    nci = nci_core_new(&hal->io);
    loop = g_main_loop_new(NULL, TRUE);
    nci_core_set_state(nci, NCI_RFST_IDLE);
    id = nci_core_add_current_state_changed_handler(nci,
        test_init_failed2_done, loop);

    timeout_id = test_setup_timeout(loop);
    g_main_loop_run(loop);
    if (timeout_id) {
        g_source_remove(timeout_id);
    }

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
typedef struct test_nci_sm_entry_set_timeout TestSmEntrySetTimeout;
typedef struct test_nci_sm_entry_send_data TestSmEntrySendData;
typedef struct test_nci_sm_entry_queue_read TestSmEntryQueueRead;
typedef struct test_nci_sm_entry_assert_states TestSmEntryAssertStates;
typedef struct test_nci_sm_entry_state TestSmEntryState;
typedef struct test_nci_sm_entry_activation TestEntryActivation;

typedef struct test_nci_sm_data {
    const char* name;
    const TestSmEntry* entries;
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
        struct test_nci_sm_entry_set_timeout {
            guint ms;
        } set_timeout;
        struct test_nci_sm_entry_send_data {
            const void* data;
            guint len;
            guint8 cid;
        } send_data;
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
        struct test_nci_sm_entry_activation {
            NCI_RF_INTERFACE rf_intf;
            NCI_PROTOCOL protocol;
            NCI_MODE mode;
        } activation;
    } data;
};

#define TEST_NCI_SM_SET_TIMEOUT(millis) { \
    .func = test_nci_sm_set_timeout, \
    .data.set_timeout = { .ms = millis } }
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
#define TEST_NCI_SM_ASSERT_STATES(current,next) { \
    .func = test_nci_sm_assert_states, \
    .data.assert_states = { .current_state = current, .next_state = next } }
#define TEST_NCI_SM_ASSERT_STATE(state) { \
    .func = test_nci_sm_assert_states, \
    .data.assert_states = { .current_state = state, .next_state = state } }
#define TEST_NCI_SM_SET_STATE(next_state) { \
    .func = test_nci_sm_set_state, \
    .data.state = { .state = next_state } }
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
    const TestSmEntrySetTimeout* timeout = &test->entry->data.set_timeout;

    GDEBUG("Timeout %u ms", timeout->ms);
    nci->cmd_timeout = timeout->ms;
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
test_nci_sm_assert_states(
    TestNciSm* test)
{
    NciCore* nci = test->nci;
    const TestSmEntryAssertStates* assert_states =
        &test->entry->data.assert_states;

    g_assert(assert_states->current_state == nci->current_state);
    g_assert(assert_states->next_state == nci->next_state);
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
    gconstpointer user_data)
{
    TestNciSm test;
    guint timeout_id;

    memset(&test, 0, sizeof(test));
    test.hal = test_hal_io_new();
    test.nci = nci_core_new(&test.hal->io);
    test.loop = g_main_loop_new(NULL, TRUE);
    test.data = user_data;
    test.entry = test.data->entries;

    test.nci->cmd_timeout = (test_opt.flags & TEST_FLAG_DEBUG) ? 0 :
        TEST_DEFAULT_CMD_TIMEOUT;
    timeout_id = test_setup_timeout(test.loop);
    while (test.entry->func) {
        test.entry->func(&test);
        test.entry++;
    }
    if (timeout_id) {
        g_source_remove(timeout_id);
    }

    nci_core_free(test.nci);
    test_hal_io_free(test.hal);
    g_main_loop_unref(test.loop);
}

/* State machine tests */

static const TestSmEntry test_nci_sm_init_ok[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_timeout[] = {
    TEST_NCI_SM_SET_TIMEOUT(500),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    /* No CORE_INIT_RSP */
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_get_config_timeout[] = {
    TEST_NCI_SM_SET_TIMEOUT(500),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    /* No CORE_GET_CONFIG_RSP */
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_set_config_timeout[] = {
    TEST_NCI_SM_SET_TIMEOUT(500),
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    /* No CORE_SET_CONFIG_RSP */
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_v2[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_v2_error[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_ERROR),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_v2_broken1[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_BROKEN1),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_v2_broken2[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_BROKEN2),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_reset_failed[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP_ERROR),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_RSP), /* Ignored */
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_reset_broken[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP_BROKEN),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_init_broken[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF), /* Unexpected (ignored) */
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP_BROKEN),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_get_config_error[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    /* GET_CONFIG errors are ignored */
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP_INVALID_PARAM),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_get_config_broken[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_BROKEN),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_ignore_unexpected_rsp[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP), /* Unexpected (ignored) */
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_failed[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP_ERROR),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_broken[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP_BROKEN),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_v2[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_NO_ROUTING),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_v2_protocol[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_v2_protocol_error[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_NO_TECHNOLOGY_ROUTING),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP_ERROR),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_v2_technology1[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP_ERROR),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_v2_technology2[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP_BROKEN),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_v2_technology3[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_NO_PROTOCOL_ROUTING),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_v2_technology4[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_NO_PROTOCOL_ROUTING),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP_BROKEN),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_v2_technology5[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_V2_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_RESET_V2_NTF),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_V2_RSP_NO_PROTOCOL_ROUTING),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_SET_LISTEN_MODE_ROUTING_RSP_ERROR),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discover_map_error[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_ERROR),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discover_map_broken[] = {
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_BROKEN),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_idle_discovery[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_GENERIC_ERROR_NTF),  /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_IGNORED_NTF),        /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_IDLE),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_NTF(RF_IGNORED_NTF),          /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(NFCEE_IGNORED_NTF),       /* Ignored */
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* And then switch back to IDLE */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_DISCOVERY, NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_IGNORED_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),

    /* And again to DISCOVERY */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_IDLE, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_idle_failed[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* And then switch back to IDLE (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_DISCOVERY, NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_ERROR),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_idle_broken[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* And then switch back to IDLE (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_DISCOVERY, NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_BROKEN),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_poll_idle[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
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
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to IDLE (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_ERROR),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_poll_idle_broken1[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to IDLE (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_BROKEN),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_poll_idle_broken2[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to IDLE (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_BROKEN),
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_poll_discovery[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_QUEUE_NTF(CORE_CONN_CREDITS_NTF),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to DISCOVERY */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
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
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_IDLE),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_dscvr_error1[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to DISCOVERY (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_ERROR), /* Fail to Discovery */

    /* Switch to to Idle instead */
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_dscvr_error2[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to DISCOVERY (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_ERROR), /* Fail to Discovery */
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_ERROR), /* Fail to Idle */
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_dscvr_error3[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to DISCOVERY (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_ERROR), /* Fail to Discovery */
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_BROKEN), /* Fail to Idle */
    TEST_NCI_SM_WAIT_STATE(NCI_STATE_ERROR),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_dscvr_broken[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Simulate activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* And then switch back to DISCOVERY (and fail) */
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP_ERROR), /* Fail to Discovery */
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
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_ASSERT_STATES(NCI_STATE_INIT, NCI_RFST_DISCOVERY),
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
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_deact_t4a[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T4A),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_ISO_DEP,
        NCI_PROTOCOL_ISO_DEP, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* Make sure these notifications don't change the state */
    TEST_NCI_SM_QUEUE_NTF(RF_IGNORED_NTF),                   /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_ERROR_NTF_BROKEN),  /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_GENERIC_ERROR_NTF), /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_TRANSMISSION_ERROR_NTF),
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_PROTOCOL_ERROR_NTF),
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_TIMEOUT_ERROR_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* Deactivate to IDLE */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_POLL_ACTIVE, NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_IDLE),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_act_error1[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Broken activation switches state machine to DISCOVERY */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2_BROKEN1),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_act_error2[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Broken activation switches state machine to DISCOVERY */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2_BROKEN2),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_act_error3[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Broken activation switches state machine to DISCOVERY */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2_BROKEN3),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_act_error4[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Broken activation switches state machine to DISCOVERY */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T4A_BROKEN1),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_deact_t4a_badparam1[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Activation */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T4A_BROKEN_ACT_PARAM1),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_ISO_DEP,
        NCI_PROTOCOL_ISO_DEP, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* Deactivate to IDLE */
    TEST_NCI_SM_SET_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_ASSERT_STATES(NCI_RFST_POLL_ACTIVE, NCI_RFST_IDLE),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_IDLE),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_dscvr_poll_deact_t4a_badparam2[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Activation (missing activation parameters, not a fatal error) */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T4A_BROKEN_ACT_PARAM2),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_ISO_DEP,
        NCI_PROTOCOL_ISO_DEP, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_ntf_t2t[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP_1),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Receive 3 discovery notifications (T2T, ISO-DEP, Proprietary) */
    TEST_NCI_SM_QUEUE_NTF(RF_DISCOVER_NTF_1_ISO_DEP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_W4_ALL_DISCOVERIES),
    TEST_NCI_SM_QUEUE_NTF(RF_DISCOVER_NTF_2_T2T),
    TEST_NCI_SM_QUEUE_NTF(RF_DISCOVER_NTF_3_PROPRIETARY_LAST),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_W4_HOST_SELECT),

    /* Select Type 2 interface */
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_SELECT_RSP),
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_T2),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_FRAME,
        NCI_PROTOCOL_T2T, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),
    TEST_NCI_SM_END()
};

static const TestSmEntry test_nci_sm_discovery_ntf_isodep[] = {
    TEST_NCI_SM_QUEUE_RSP(CORE_RESET_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_INIT_RSP_1),
    TEST_NCI_SM_QUEUE_RSP(CORE_GET_CONFIG_RSP),
    TEST_NCI_SM_QUEUE_RSP(CORE_SET_CONFIG_RSP),

    /* Switch state machine to DISCOVERY state */
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_MAP_RSP),
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_RSP),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_DISCOVERY),

    /* Receive 2 discovery notifications (ISO-DEP, and Proprietary) */
    TEST_NCI_SM_QUEUE_NTF(RF_DISCOVER_NTF_1_ISO_DEP),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_W4_ALL_DISCOVERIES),
    TEST_NCI_SM_QUEUE_NTF(RF_DISCOVER_NTF_2_PROPRIETARY_LAST),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_W4_HOST_SELECT),

    /* Make sure these notifications don't change the state */
    TEST_NCI_SM_QUEUE_NTF(RF_IGNORED_NTF),                   /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_IGNORED_NTF),                 /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(NFCEE_IGNORED_NTF),                /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_GENERIC_ERROR_NTF),           /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_GENERIC_ERROR_NTF_BROKEN),    /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(CORE_GENERIC_TARGET_ACTIVATION_FAILED_ERROR_NTF),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_W4_HOST_SELECT),

    /* Select ISO-DEP interface */
    TEST_NCI_SM_QUEUE_RSP(RF_DISCOVER_SELECT_RSP),
    TEST_NCI_SM_QUEUE_NTF(CORE_IGNORED_NTF),  /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(RF_IGNORED_NTF),    /* Ignored */
    TEST_NCI_SM_QUEUE_NTF(RF_INTF_ACTIVATED_NTF_ISO_DEP),
    TEST_NCI_SM_WAIT_ACTIVATION(NCI_RF_INTERFACE_ISO_DEP,
        NCI_PROTOCOL_ISO_DEP, NCI_MODE_PASSIVE_POLL_A),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_POLL_ACTIVE),

    /* Try to deactivate to DISCOVERY */
    TEST_NCI_SM_QUEUE_NTF(CORE_INTERFACE_TIMEOUT_ERROR_NTF),
    TEST_NCI_SM_SET_STATE(NCI_RFST_DISCOVERY),
    TEST_NCI_SM_QUEUE_RSP(RF_DEACTIVATE_RSP),

    /* But end up in IDLE */
    TEST_NCI_SM_QUEUE_NTF(RF_DEACTIVATE_NTF_IDLE),
    TEST_NCI_SM_WAIT_STATE(NCI_RFST_IDLE),
    TEST_NCI_SM_END()
};

const const TestNciSmData nci_sm_tests[] = {
    { "init-ok", test_nci_sm_init_ok },
    { "init-timeout", test_nci_sm_init_timeout },
    { "get-config-timeout", test_nci_sm_get_config_timeout },
    { "set-config-timeout", test_nci_sm_set_config_timeout },
    { "init-v2", test_nci_sm_init_v2 },
    { "init-v2-error", test_nci_sm_init_v2_error },
    { "init-v2-broken1", test_nci_sm_init_v2_broken1 },
    { "init-v2-broken2", test_nci_sm_init_v2_broken2 },
    { "reset-failed", test_nci_sm_reset_failed },
    { "reset-broken", test_nci_sm_reset_broken },
    { "init-broken", test_nci_sm_init_broken },
    { "get-config-error", test_nci_sm_get_config_error },
    { "get-config-broken", test_nci_sm_get_config_broken },
    { "ignore-unexpected-rsp", test_nci_sm_ignore_unexpected_rsp },
    { "discovery-failed", test_nci_sm_discovery_failed },
    { "discovery-broken", test_nci_sm_discovery_broken },
    { "discovery-v2", test_nci_sm_discovery_v2 },
    { "discovery-v2-protocol", test_nci_sm_discovery_v2_protocol },
    { "discovery-v2-protocol-error", test_nci_sm_discovery_v2_protocol_error },
    { "discovery-v2-technology1", test_nci_sm_discovery_v2_technology1 },
    { "discovery-v2-technology2", test_nci_sm_discovery_v2_technology2 },
    { "discovery-v2-technology3", test_nci_sm_discovery_v2_technology3 },
    { "discovery-v2-technology4", test_nci_sm_discovery_v2_technology4 },
    { "discovery-v2-technology5", test_nci_sm_discovery_v2_technology5 },
    { "discovery-discover-map-error", test_nci_sm_discover_map_error },
    { "discovery-discover-map-broken", test_nci_sm_discover_map_broken },
    { "discovery-idle-discovery", test_nci_sm_discovery_idle_discovery },
    { "discovery-idle-failed", test_nci_sm_discovery_idle_failed },
    { "discovery-idle-broken", test_nci_sm_discovery_idle_broken },
    { "discovery-poll-idle", test_nci_sm_discovery_poll_idle },
    { "discovery-poll-idle-failed", test_nci_sm_discovery_poll_idle_failed },
    { "discovery-poll-idle-broken1", test_nci_sm_discovery_poll_idle_broken1 },
    { "discovery-poll-idle-broken2", test_nci_sm_discovery_poll_idle_broken2 },
    { "discovery-poll-discovery", test_nci_sm_discovery_poll_discovery },
    { "discovery-poll-discovery-error1", test_nci_sm_dscvr_poll_dscvr_error1 },
    { "discovery-poll-discovery-error2", test_nci_sm_dscvr_poll_dscvr_error2 },
    { "discovery-poll-discovery-error3", test_nci_sm_dscvr_poll_dscvr_error3 },
    { "discovery-poll-discovery-broken", test_nci_sm_dscvr_poll_dscvr_broken },
    { "discovery-poll-read-discovery", test_nci_sm_dscvr_poll_read_dscvr },
    { "discovery-poll-deactivate-t4a", test_nci_sm_dscvr_poll_deact_t4a },
    { "discovery-poll-activate-error1",  test_nci_sm_dscvr_poll_act_error1 },
    { "discovery-poll-activate-error2",  test_nci_sm_dscvr_poll_act_error2 },
    { "discovery-poll-activate-error3",  test_nci_sm_dscvr_poll_act_error3 },
    { "discovery-poll-activate-error4",  test_nci_sm_dscvr_poll_act_error4 },
    { "discovery-poll-deactivate-t4a-bad-act-param1",
       test_nci_sm_dscvr_poll_deact_t4a_badparam1 },
    { "discovery-ntf-t2t", test_nci_sm_discovery_ntf_t2t },
    { "discovery-ntf-isodep", test_nci_sm_discovery_ntf_isodep }
};

/*==========================================================================*
 * Common
 *==========================================================================*/

#define TEST_PREFIX "/nci_core/"
#define TEST_(name) TEST_PREFIX name

int main(int argc, char* argv[])
{
    guint i;

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
