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

#include "nci_types_p.h"
#include "nci_hal.h"
#include "nci_sar.h"

#include <gutil_macros.h>
#include <gutil_log.h>

static TestOpt test_opt;

#define TEST_TIMEOUT (10) /* seconds */

#define TEST_GID  (0x01)
#define TEST_OID  (0x02)

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

static
void
test_client_unexpected_completion(
    NciSarClient* client,
    gboolean success,
    gpointer user_data)
{
    g_assert(FALSE);
}

static
void
test_client_expect_success(
    NciSarClient* client,
    gboolean success,
    gpointer user_data)
{
    g_assert(success);
}

static
void
test_client_expect_error(
    NciSarClient* client,
    gboolean success,
    gpointer user_data)
{
    g_assert(!success);
}

static
void
test_sar_client_unexpected(
    NciSarClient* client)
{
    g_assert(FALSE);
}

static
void
test_sar_client_unexpected_resp(
    NciSarClient* client,
    guint8 gid,
    guint8 oid,
    const void* payload,
    guint payload_len)
{
    g_assert(FALSE);
}

static
void
test_sar_client_unexpected_data_packet(
    NciSarClient* client,
    guint8 cid,
    const void* payload,
    guint payload_len)
{
    g_assert(FALSE);
}

/*==========================================================================*
 * Test HAL
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
    GPtrArray* written;
    guint write_id;
    NciHalClient* sar;
    void* test_data;
} TestHalIo;

typedef struct test_hal_io_write {
    TestHalIo* hal;
    GBytes* bytes;
    NciHalClientFunc complete;
} TestHalIoWrite;

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
    return G_SOURCE_REMOVE;
}

static
gboolean
test_hal_io_start(
    NciHalIo* io,
    NciHalClient* sar)
{
    TestHalIo* hal = G_CAST(io, TestHalIo, io);

    g_assert(!hal->sar);
    hal->sar = sar;
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
test_hal_io_free(
   TestHalIo* hal)
{
    g_assert(!hal->sar);
    g_assert(!hal->write_id);
    g_ptr_array_free(hal->written, TRUE);
    g_free(hal);
}

/*==========================================================================*
 * Test SAR client
 *==========================================================================*/

static
void
test_dummy_sar_client_error(
    NciSarClient* client)
{
}

static
void
test_dummy_sar_client_handle_response(
    NciSarClient* client,
    guint8 gid,
    guint8 oid,
    const void* payload,
    guint payload_len)
{
}

static
void
test_dummy_sar_client_handle_notification(
    NciSarClient* client,
    guint8 gid,
    guint8 oid,
    const void* payload,
    guint payload_len)
{
}

static
void
test_dummy_sar_client_handle_data_packet(
    NciSarClient* client,
    guint8 cid,
    const void* payload,
    guint payload_len)
{
}

static const NciSarClientFunctions test_dummy_sar_client_fn = {
    .error = test_dummy_sar_client_error,
    .handle_response = test_dummy_sar_client_handle_response,
    .handle_notification = test_dummy_sar_client_handle_notification,
    .handle_data_packet = test_dummy_sar_client_handle_data_packet
};

/*==========================================================================*
 * null
 *==========================================================================*/

static
void
test_null(
    void)
{
    nci_sar_set_max_logical_connections(NULL, 0);
    nci_sar_set_max_control_packet_size(NULL, 0);
    g_assert(!nci_sar_send_command(NULL, 0, 0, NULL, NULL, NULL, NULL));
    g_assert(!nci_sar_send_data_packet(NULL, 0, NULL, NULL, NULL, NULL));
    g_assert(!nci_sar_start(NULL));
    nci_sar_set_initial_credits(NULL, 0, 1);
    nci_sar_add_credits(NULL, 0, 1);
    nci_sar_reset(NULL);
    nci_sar_cancel(NULL, 0);
    nci_sar_free(NULL);
}

/*==========================================================================*
 * fail
 *==========================================================================*/

typedef struct test_fail_client {
    NciSarClient client;
    GMainLoop* loop;
} TestFailClient;

static
void
test_fail_expect_error_and_quit(
    NciSarClient* client,
    gboolean success,
    gpointer user_data)
{
    TestFailClient* test = G_CAST(client, TestFailClient, client);

    g_assert(!success);
    g_main_loop_quit(test->loop);
}

static
void
test_fail(
    void)
{
    NciSar* sar;
    TestFailClient test;
    guint timeout_id;

    memset(&test, 0, sizeof(test));
    test.client.fn = &test_dummy_sar_client_fn;

    sar = nci_sar_new(&test_dummy_hal_io, &test.client);
    nci_sar_set_initial_credits(sar, 0, 1);
    g_assert(nci_sar_send_command(sar, 0, 0, NULL, NULL, NULL, NULL));
    g_assert(nci_sar_send_command(sar, 0, 0, NULL,
        test_client_expect_error, NULL, NULL));
    g_assert(nci_sar_send_data_packet(sar, 0, NULL,
        test_fail_expect_error_and_quit, NULL, NULL));
    nci_sar_cancel(sar, 0); /* Does nothing */
    nci_sar_cancel(sar, 112345); /* Invalid ID */

    test.loop = g_main_loop_new(NULL, TRUE);
    timeout_id = test_setup_timeout(test.loop);
    g_main_loop_run(test.loop);
    if (timeout_id) {
        g_source_remove(timeout_id);
    }

    nci_sar_free(sar);
    g_main_loop_unref(test.loop);
}

/*==========================================================================*
 * basic
 *==========================================================================*/

typedef struct test_basic_client {
    NciSarClient client;
    int error_count;
    GMainLoop* loop;
} TestBasicClient;

static
void
test_basic_sar_client_error(
    NciSarClient* client)
{
    G_CAST(client, TestBasicClient, client)->error_count++;
}

static
void
test_basic_expect_success_and_quit(
    NciSarClient* client,
    gboolean success,
    gpointer user_data)
{
    TestBasicClient* test = G_CAST(client, TestBasicClient, client);

    g_assert(success);
    g_main_loop_quit(test->loop);
}

static
void
test_basic(
    void)
{
    static const NciSarClientFunctions test_basic_sar_client_fn = {
        .error = test_basic_sar_client_error,
        .handle_response = test_dummy_sar_client_handle_response,
        .handle_notification = test_dummy_sar_client_handle_notification,
        .handle_data_packet = test_dummy_sar_client_handle_data_packet
    };

    NciSar* sar;
    TestHalIo* test_io = test_hal_io_new();
    TestBasicClient test;
    guint timeout_id;

    memset(&test, 0, sizeof(test));
    test.client.fn = &test_basic_sar_client_fn;

    sar = nci_sar_new(&test_io->io, &test.client);
    nci_sar_set_max_logical_connections(sar, 0);
    nci_sar_set_max_logical_connections(sar, 1);
    nci_sar_set_max_logical_connections(sar, 3);
    nci_sar_set_max_logical_connections(sar, 2);
    nci_sar_set_max_control_packet_size(sar, 0);
    nci_sar_set_max_control_packet_size(sar, 0xff);
    nci_sar_set_initial_credits(sar, 42, 1); /* Invalid cid */
    nci_sar_set_initial_credits(sar, 0, 0xfe);
    nci_sar_add_credits(sar, 0, 1);
    nci_sar_add_credits(sar, 0, 1); /* one too many */
    g_assert(nci_sar_send_command(sar, TEST_GID, TEST_OID, NULL,
        NULL, NULL, NULL));
    g_assert(nci_sar_send_command(sar, TEST_GID, TEST_OID, NULL,
        test_client_expect_success, NULL, NULL));
    g_assert(nci_sar_send_data_packet(sar, 0, NULL,
        test_basic_expect_success_and_quit, NULL, NULL));
    nci_sar_add_credits(sar, 0, 1); /* No effect */
    nci_sar_add_credits(sar, 42, 1); /* Invalid cid */

    test.loop = g_main_loop_new(NULL, TRUE);
    timeout_id = test_setup_timeout(test.loop);
    g_main_loop_run(test.loop);

    /* Signal error */
    g_assert(test_io->sar);
    test_io->sar->fn->error(test_io->sar);
    g_assert(test.error_count == 1);
    nci_sar_reset(sar);

    g_assert(test_io->written->len == 3);
    if (timeout_id) {
        g_source_remove(timeout_id);
    }

    nci_sar_free(sar);
    test_hal_io_free(test_io);
    g_main_loop_unref(test.loop);
}

/*==========================================================================*
 * send_seg
 *==========================================================================*/

typedef struct test_send_seg_data {
    NciSarClient client;
    NciSar* sar;
    int error_count;
    GMainLoop* loop;
    guint send_id;
} TestSendSegData;

static
void
test_send_seg_expect_success_and_quit(
    NciSarClient* client,
    gboolean success,
    gpointer user_data)
{
    TestSendSegData* test = G_CAST(client, TestSendSegData, client);

    g_assert(success);
    g_main_loop_quit(test->loop);
}

static
gboolean
test_send_seg_hal_io_write(
    NciHalIo* io,
    const GUtilData* chunks,
    guint count,
    NciHalClientFunc complete)
{
    TestHalIo* hal = G_CAST(io, TestHalIo, io);
    TestSendSegData* test = hal->test_data;

    g_assert(test_hal_io_write(io, chunks, count, complete));

    /* Both packets will still be sent (because we cancel this one after
     * we have already started writing it), it's just that the completion
     * callback won't be invoked for the first packet. Note that we call
     * cancel many times, but only the first time it really does something. */
    nci_sar_cancel(test->sar, test->send_id);
    return TRUE;
}

static
void
test_send_seg(
    void)
{
    static const NciHalIoFunctions hal_io_fn = {
        .start = test_hal_io_start,
        .stop = test_hal_io_stop,
        .write = test_send_seg_hal_io_write,
        .cancel_write = test_hal_io_cancel_write
    };

    /* Set MTU to minimum and send one byte of payload per packet */
    static const guint8 payload[] = { 0x01, 0x02 };
    GBytes* payload_bytes = g_bytes_new_static(payload, sizeof(payload));
    TestHalIo* test_io = test_hal_io_new_with_functions(&hal_io_fn);
    TestSendSegData test;
    guint timeout_id, i;

    memset(&test, 0, sizeof(test));
    test.client.fn = &test_dummy_sar_client_fn;
    test_io->test_data = &test;

    test.sar = nci_sar_new(&test_io->io, &test.client);
    nci_sar_set_max_control_packet_size(test.sar, 3); /* Actually sets 4 */
    test.send_id = nci_sar_send_command(test.sar, TEST_GID, TEST_OID,
        payload_bytes, test_client_unexpected_completion, NULL, NULL);
    g_assert(test.send_id);
    g_assert(nci_sar_send_command(test.sar, TEST_GID, TEST_OID,
        payload_bytes, test_send_seg_expect_success_and_quit, NULL, NULL));

    test.loop = g_main_loop_new(NULL, TRUE);
    timeout_id = test_setup_timeout(test.loop);
    g_main_loop_run(test.loop);

    /* The same data have been sent twice */
    g_assert(test_io->written->len == 2*sizeof(payload));
    for (i = 0; i < test_io->written->len; i++) {
        GBytes* packet = test_io->written->pdata[i];
        gsize packet_size;
        const guint8* packet_data = g_bytes_get_data(packet, &packet_size);

        g_assert(packet_size == 4);
        g_assert(packet_data[3] == payload[i % sizeof(payload)]);

    }
    if (timeout_id) {
        g_source_remove(timeout_id);
    }

    nci_sar_free(test.sar);
    test_hal_io_free(test_io);
    g_main_loop_unref(test.loop);
    g_bytes_unref(payload_bytes);
}

/*==========================================================================*
 * send_err
 *==========================================================================*/

typedef struct test_send_err_hal_io {
    NciHalIo io;
    NciHalClient* sar;
} TestSendErrHalIo;

typedef struct test_send_err_hal_io_write {
    NciHalClient* sar;
    NciHalClientFunc complete;
} TestSendErrHalIoWrite;

static
gboolean
test_send_err_hal_io_start(
    NciHalIo* io,
    NciHalClient* sar)
{
    TestSendErrHalIo* hal = G_CAST(io, TestSendErrHalIo, io);

    g_assert(!hal->sar);
    hal->sar = sar;
    return TRUE;
}

static
gboolean
test_send_err_hal_io_write_cb(
    gpointer user_data)
{
    TestSendErrHalIoWrite* write = user_data;

    write->complete(write->sar, FALSE);
    return G_SOURCE_REMOVE;
}

static
gboolean
test_send_err_hal_io_write(
    NciHalIo* io,
    const GUtilData* chunks,
    guint count,
    NciHalClientFunc complete)
{
    TestSendErrHalIo* hal = G_CAST(io, TestSendErrHalIo, io);
    TestSendErrHalIoWrite* write = g_new0(TestSendErrHalIoWrite, 1);

    write->complete = complete;
    write->sar = hal->sar;
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, test_send_err_hal_io_write_cb,
        write, g_free);
    return TRUE;
}

static
TestSendErrHalIo*
test_send_err_hal_io_new(
    void)
{
    static const NciHalIoFunctions test_dummy_hal_io_fn = {
        .start = test_send_err_hal_io_start,
        .stop = test_dummy_hal_io_stop,
        .write = test_send_err_hal_io_write,
        .cancel_write = test_dummy_hal_io_cancel_write
    };
    TestSendErrHalIo* hal = g_new0(TestSendErrHalIo, 1);

    hal->io.fn = &test_dummy_hal_io_fn;
    return hal;
}

static
void
test_send_err_hal_io(
    TestSendErrHalIo* hal)
{
    g_free(hal);
}

typedef struct test_send_err_sar_client {
    NciSarClient client;
    GMainLoop* loop;
    gboolean completed;
} TestSendErrSarClient;

static
void
test_send_err_sar_send_complete(
    NciSarClient* client,
    gboolean success,
    gpointer user_data)
{
    TestSendErrSarClient* test = user_data;

    g_assert(&test->client == client);
    g_assert(!success);
    g_assert(!test->completed);
    test->completed = TRUE;
}

static
void
test_send_err_sar_client_error(
    NciSarClient* client)
{
    TestSendErrSarClient* test = G_CAST(client, TestSendErrSarClient, client);

    g_main_loop_quit(test->loop);
}

static
void
test_send_err(
    void)
{
    static const NciSarClientFunctions test_send_err_sar_client_fn = {
        .error = test_send_err_sar_client_error,
        .handle_response = test_dummy_sar_client_handle_response,
        .handle_notification = test_dummy_sar_client_handle_notification,
        .handle_data_packet = test_dummy_sar_client_handle_data_packet
    };
    TestSendErrHalIo* test_io = test_send_err_hal_io_new();
    TestSendErrSarClient test;
    NciSar* sar;
    guint timeout_id;

    memset(&test, 0, sizeof(test));
    test.client.fn = &test_send_err_sar_client_fn;
    test.loop = g_main_loop_new(NULL, TRUE);
    timeout_id = test_setup_timeout(test.loop);

    sar = nci_sar_new(&test_io->io, &test.client);
    g_assert(nci_sar_start(sar));
    g_assert(test_io->sar);

    /* This send fails asynchronously */
    g_assert(nci_sar_send_command(sar, TEST_GID, TEST_OID, NULL,
        NULL, NULL, NULL));

    g_main_loop_run(test.loop);

    /* Same thing but with a completion callback */
    g_assert(nci_sar_send_command(sar, TEST_GID, TEST_OID, NULL,
        test_send_err_sar_send_complete, NULL, &test));

    g_main_loop_run(test.loop);
    if (timeout_id) {
        g_source_remove(timeout_id);
    }

    nci_sar_free(sar);
    test_send_err_hal_io(test_io);
    g_main_loop_unref(test.loop);
}

/*==========================================================================*
 * recv_ntf
 *==========================================================================*/

typedef struct test_recv_ntf {
    NciSarClient client;
    int ntf_received;
} TestRecvNtf;

static
void
test_recv_ntf_handle(
    NciSarClient* client,
    guint8 gid,
    guint8 oid,
    const void* payload,
    guint payload_len)
{
    TestRecvNtf* test = G_CAST(client, TestRecvNtf, client);

    g_assert(gid == TEST_GID);
    g_assert(oid == TEST_OID);
    g_assert(!payload_len);
    test->ntf_received++;
}

static
void
test_recv_ntf(
    void)
{
    static const NciSarClientFunctions test_recv_ntf_sar_client_fn = {
        .error = test_dummy_sar_client_error,
        .handle_response = test_dummy_sar_client_handle_response,
        .handle_notification = test_recv_ntf_handle,
        .handle_data_packet = test_dummy_sar_client_handle_data_packet
    };
    static const guint8 ntf[] = { NCI_MT_NTF_PKT | TEST_GID, TEST_OID, 0 };
    NciSar* sar;
    TestHalIo* test_io = test_hal_io_new();
    TestRecvNtf test;

    memset(&test, 0, sizeof(test));
    test.client.fn = &test_recv_ntf_sar_client_fn;

    sar = nci_sar_new(&test_io->io, &test.client);
    g_assert(nci_sar_start(sar));
    g_assert(nci_sar_start(sar)); /* Double start is handled */
    g_assert(test_io->sar);

    /* Test reception of a complete (non-segmented) packet */
    test_io->sar->fn->read(test_io->sar, ntf, sizeof(ntf));
    g_assert(test.ntf_received == 1);

    nci_sar_free(sar);
    test_hal_io_free(test_io);
}

/*==========================================================================*
 * recv_ntf_data
 *==========================================================================*/

typedef struct test_recv_ntf_data {
    NciSarClient client;
    int ntf_received;
} TestRecvNtfData;

static const guint8 test_recv_ntf_data_payload[] = { 0x01, 0x02, 0x03 };

static
void
test_recv_ntf_data_handle(
    NciSarClient* client,
    guint8 gid,
    guint8 oid,
    const void* payload,
    guint payload_len)
{
    TestRecvNtfData* test = G_CAST(client, TestRecvNtfData, client);

    g_assert(gid == TEST_GID);
    g_assert(oid == TEST_OID);
    g_assert(payload_len == sizeof(test_recv_ntf_data_payload));
    g_assert(!memcmp(test_recv_ntf_data_payload, payload, payload_len));
    test->ntf_received++;
}

static
void
test_recv_ntf_data(
    void)
{
    static const NciSarClientFunctions test_recv_ntf_sar_client_fn = {
        .error = test_dummy_sar_client_error,
        .handle_response = test_dummy_sar_client_handle_response,
        .handle_notification = test_recv_ntf_data_handle,
        .handle_data_packet = test_dummy_sar_client_handle_data_packet
    };
    static const guint8 hdr[] = { NCI_MT_NTF_PKT | TEST_GID, TEST_OID };
    NciSar* sar;
    TestHalIo* test_io = test_hal_io_new();
    GByteArray* packet = g_byte_array_new();
    TestRecvNtfData test;
    guint8 payload_len = sizeof(test_recv_ntf_data_payload);

    g_byte_array_append(packet, hdr, sizeof(hdr));
    g_byte_array_append(packet, &payload_len, 1);
    g_byte_array_append(packet, test_recv_ntf_data_payload,
        sizeof(test_recv_ntf_data_payload));

    memset(&test, 0, sizeof(test));
    test.client.fn = &test_recv_ntf_sar_client_fn;

    sar = nci_sar_new(&test_io->io, &test.client);
    g_assert(nci_sar_start(sar));
    g_assert(test_io->sar);

    /* Test reception of a complete (non-segmented) packet */
    test_io->sar->fn->read(test_io->sar, packet->data, packet->len);
    g_assert(test.ntf_received == 1);

    nci_sar_free(sar);
    test_hal_io_free(test_io);
    g_byte_array_free(packet, TRUE);
}

/*==========================================================================*
 * recv_multi
 *==========================================================================*/

typedef struct test_recv_multi {
    NciSarClient client;
    int ntf_received;
    int rsp_received;
} TestRecvMulti;

static
void
test_recv_multi_handle_resp(
    NciSarClient* client,
    guint8 gid,
    guint8 oid,
    const void* payload,
    guint payload_len)
{
    TestRecvMulti* test = G_CAST(client, TestRecvMulti, client);

    g_assert(gid == TEST_GID);
    g_assert(oid == TEST_OID);
    g_assert(!payload_len);
    test->rsp_received++;
}

static
void
test_recv_multi_handle_ntf(
    NciSarClient* client,
    guint8 gid,
    guint8 oid,
    const void* payload,
    guint payload_len)
{
    TestRecvMulti* test = G_CAST(client, TestRecvMulti, client);

    g_assert(gid == TEST_GID);
    g_assert(oid == TEST_OID);
    g_assert(!payload_len);
    test->ntf_received++;
}

static
void
test_recv_multi(
    void)
{
    static const NciSarClientFunctions test_recv_multi_fn = {
        .error = test_dummy_sar_client_error,
        .handle_response = test_recv_multi_handle_resp,
        .handle_notification = test_recv_multi_handle_ntf,
        .handle_data_packet = test_dummy_sar_client_handle_data_packet
    };
    static const guint8 packets[] = {
        NCI_MT_RSP_PKT | TEST_GID, TEST_OID, 0,
        NCI_MT_NTF_PKT | TEST_GID, TEST_OID, 0,
        NCI_MT_CMD_PKT | TEST_GID, TEST_OID, 0 /* This one is ignored */
    };
    NciSar* sar;
    TestHalIo* test_io = test_hal_io_new();
    TestRecvMulti test;

    memset(&test, 0, sizeof(test));
    test.client.fn = &test_recv_multi_fn;

    sar = nci_sar_new(&test_io->io, &test.client);
    g_assert(nci_sar_start(sar));
    g_assert(test_io->sar);

    test_io->sar->fn->read(test_io->sar, packets, sizeof(packets));
    g_assert(test.ntf_received == 1);
    g_assert(test.rsp_received == 1);

    nci_sar_free(sar);
    test_hal_io_free(test_io);
}

/*==========================================================================*
 * recv_seg
 *==========================================================================*/

typedef struct test_recv_seg {
    NciSarClient client;
    int ntf_received;
    int rsp_received;
} TestRecvSeg;

static
void
test_recv_seg_handle_resp(
    NciSarClient* client,
    guint8 gid,
    guint8 oid,
    const void* payload,
    guint payload_len)
{
    TestRecvSeg* test = G_CAST(client, TestRecvSeg, client);

    g_assert(gid == TEST_GID);
    g_assert(oid == TEST_OID);
    g_assert(!payload_len);
    test->rsp_received++;
}

static
void
test_recv_seg_handle_ntf(
    NciSarClient* client,
    guint8 gid,
    guint8 oid,
    const void* payload,
    guint payload_len)
{
    TestRecvSeg* test = G_CAST(client, TestRecvSeg, client);
    static const guint8 expected_data[] = { 0x01, 0x02, 0x03 };

    g_assert(gid == TEST_GID);
    g_assert(oid == TEST_OID);
    g_assert(payload_len == sizeof(expected_data));
    g_assert(!memcmp(expected_data, payload, payload_len));
    test->ntf_received++;
}

static
void
test_recv_seg(
    void)
{
    static const NciSarClientFunctions test_recv_seg_fn = {
        .error = test_sar_client_unexpected,
        .handle_response = test_recv_seg_handle_resp,
        .handle_notification = test_recv_seg_handle_ntf,
        .handle_data_packet = test_sar_client_unexpected_data_packet
    };
    static const guint8 buf1[] = {
        NCI_MT_RSP_PKT | TEST_GID, TEST_OID,
    };
    static const guint8 buf2[] = {
        0, /* End of the first packet */
        NCI_MT_NTF_PKT | TEST_GID | NCI_PBF, TEST_OID, 1, 0x01
    };
    static const guint8 buf3[] = {
        NCI_MT_NTF_PKT | TEST_GID, TEST_OID, 2, 0x02
    };
    static const guint8 buf4[] = {
        0x03 /* End of the third packet */
    };
    NciSar* sar;
    TestHalIo* test_io = test_hal_io_new();
    TestRecvSeg test;

    memset(&test, 0, sizeof(test));
    test.client.fn = &test_recv_seg_fn;

    sar = nci_sar_new(&test_io->io, &test.client);
    g_assert(nci_sar_start(sar));
    g_assert(test_io->sar);

    test_io->sar->fn->read(test_io->sar, buf1, sizeof(buf1));
    test_io->sar->fn->read(test_io->sar, buf2, sizeof(buf2));
    test_io->sar->fn->read(test_io->sar, buf3, sizeof(buf3));
    test_io->sar->fn->read(test_io->sar, buf4, sizeof(buf4));
    g_assert(test.ntf_received == 1);
    g_assert(test.rsp_received == 1);

    nci_sar_free(sar);
    test_hal_io_free(test_io);
}

/*==========================================================================*
 * recv_seg_err
 *==========================================================================*/

typedef struct test_recv_seg_err {
    NciSarClient client;
    gboolean error;
} TestRecvSegErr;

static
void
test_recv_seg_err_client_error(
    NciSarClient* client)
{
    TestRecvSegErr* test = G_CAST(client, TestRecvSegErr, client);

    test->error = TRUE;
}

static
void
test_recv_seg_err(
    void)
{
    static const NciSarClientFunctions test_recv_seg_err_fn = {
        .error = test_recv_seg_err_client_error,
        .handle_response = test_sar_client_unexpected_resp,
        .handle_notification = test_sar_client_unexpected_resp,
        .handle_data_packet = test_sar_client_unexpected_data_packet
    };
    static const guint8 buf1[] = {
        NCI_MT_NTF_PKT | TEST_GID | NCI_PBF, TEST_OID, 1, 0x01
    };
    static const guint8 buf2[] = {
        NCI_MT_RSP_PKT | TEST_GID, TEST_OID, 2, 0x02, 0x03
    };
    static const guint8 buf3[] = {
        NCI_MT_NTF_PKT | (TEST_GID + 1), TEST_OID, 2, 0x02, 0x03
    };
    static const guint8 buf4[] = {
        NCI_MT_NTF_PKT | TEST_GID, TEST_OID + 1, 2, 0x02, 0x03
    };

    NciSar* sar;
    TestHalIo* test_io = test_hal_io_new();
    TestRecvSegErr test;

    memset(&test, 0, sizeof(test));
    test.client.fn = &test_recv_seg_err_fn;

    sar = nci_sar_new(&test_io->io, &test.client);
    g_assert(nci_sar_start(sar));
    g_assert(test_io->sar);

    test_io->sar->fn->read(test_io->sar, buf1, sizeof(buf1));
    g_assert(!test.error);

    /* Packet type mismatch */
    test_io->sar->fn->read(test_io->sar, buf2, sizeof(buf2));
    g_assert(test.error);

    /* GID mismatch */
    test.error = FALSE;
    test_io->sar->fn->read(test_io->sar, buf3, sizeof(buf3));
    g_assert(test.error);

    /* OID mismatch */
    test.error = FALSE;
    test_io->sar->fn->read(test_io->sar, buf4, sizeof(buf4));
    g_assert(test.error);

    nci_sar_free(sar);
    test_hal_io_free(test_io);
}

/*==========================================================================*
 * bad_cid
 *==========================================================================*/

typedef struct test_error_count {
    NciSarClient client;
    int error_count;
} TestErrorCount;

static
void
test_sar_client_error_count(
    NciSarClient* client)
{
    TestErrorCount* test = G_CAST(client, TestErrorCount, client);

    test->error_count++;
}

static
void
test_bad_cid(
    void)
{
    static const NciSarClientFunctions test_bad_cid_fn = {
        .error = test_sar_client_error_count,
        .handle_response = test_sar_client_unexpected_resp,
        .handle_notification = test_sar_client_unexpected_resp,
        .handle_data_packet = test_sar_client_unexpected_data_packet
    };
    static const guint8 buf[] = {
        NCI_MT_DATA_PKT | 1, 0, 0, 0
    };
    NciSar* sar;
    TestHalIo* test_io = test_hal_io_new();
    TestErrorCount test;

    memset(&test, 0, sizeof(test));
    test.client.fn = &test_bad_cid_fn;

    sar = nci_sar_new(&test_io->io, &test.client);
    g_assert(nci_sar_start(sar));
    g_assert(test_io->sar);

    nci_sar_set_max_logical_connections(sar, 1);
    test_io->sar->fn->read(test_io->sar, buf, sizeof(buf));
    g_assert(test.error_count == 1);

    nci_sar_free(sar);
    test_hal_io_free(test_io);
}

/*==========================================================================*
 * recv_data
 *==========================================================================*/

typedef struct test_recv_data {
    NciSarClient client;
    int packet_count;
} TestRecvData;

static
void
test_recv_data_handle_packet(
    NciSarClient* client,
    guint8 cid,
    const void* payload,
    guint payload_len)
{
    TestRecvData* test = G_CAST(client, TestRecvData, client);
    static const guint8 payload1[] = { 0x01 };
    static const guint8 payload2[] = { 0x02, 0x03 };

    GDEBUG("packet #%d, %u byte(s)", test->packet_count, payload_len);
    switch (test->packet_count) {
    case 0:
        g_assert(cid == 0);
        g_assert(payload_len == 0);
        break;
    case 1:
        g_assert(cid == 1);
        g_assert(payload_len == sizeof(payload1));
        g_assert(!memcmp(payload, payload1, payload_len));
        break;
    case 2:
        g_assert(cid == 1);
        g_assert(payload_len == sizeof(payload2));
        g_assert(!memcmp(payload, payload2, payload_len));
        break;
    default: g_assert(FALSE);
    }
    test->packet_count++;
}

static
void
test_recv_data(
    void)
{
    static const NciSarClientFunctions test_recv_data_fn = {
        .error = test_dummy_sar_client_error,
        .handle_response = test_sar_client_unexpected_resp,
        .handle_notification = test_sar_client_unexpected_resp,
        .handle_data_packet = test_recv_data_handle_packet
    };
    static const guint8 buf1[] = {
        NCI_MT_DATA_PKT, 0,
    };
    static const guint8 buf2[] = {
        0, /* End of the first packet */
        NCI_MT_DATA_PKT | 1, 0, 1, 0x01
    };
    static const guint8 buf3[] = {
        NCI_MT_DATA_PKT | 1, 0, 2, 0x02
    };
    static const guint8 buf4[] = {
        0x03 /* End of the third packet */
    };
    NciSar* sar;
    TestHalIo* test_io = test_hal_io_new();
    TestRecvData test;

    memset(&test, 0, sizeof(test));
    test.client.fn = &test_recv_data_fn;

    sar = nci_sar_new(&test_io->io, &test.client);
    g_assert(nci_sar_start(sar));
    g_assert(test_io->sar);

    nci_sar_set_max_logical_connections(sar, 2);
    test_io->sar->fn->read(test_io->sar, buf1, sizeof(buf1));
    test_io->sar->fn->read(test_io->sar, buf2, sizeof(buf2));
    test_io->sar->fn->read(test_io->sar, buf3, sizeof(buf3));
    test_io->sar->fn->read(test_io->sar, buf4, sizeof(buf4));
    g_assert(test.packet_count == 3);

    nci_sar_free(sar);
    test_hal_io_free(test_io);
}

/*==========================================================================*
 * recv_data_seg
 *==========================================================================*/

typedef struct test_recv_data_seg {
    NciSarClient client;
    gboolean packet_received;
} TestRecvDataSeg;

static
void
test_recv_data_seg_handle_packet(
    NciSarClient* client,
    guint8 cid,
    const void* payload,
    guint payload_len)
{
    TestRecvDataSeg* test = G_CAST(client, TestRecvDataSeg, client);
    static const guint8 expected_payload[] = { 0x01, 0x02, 0x03 };

    g_assert(cid == 1);
    g_assert(payload_len == sizeof(expected_payload));
    g_assert(!memcmp(payload, expected_payload, payload_len));
    g_assert(!test->packet_received);
    test->packet_received = TRUE;
}

static
void
test_recv_data_seg(
    void)
{
    static const NciSarClientFunctions test_recv_data_seg_fn = {
        .error = test_dummy_sar_client_error,
        .handle_response = test_sar_client_unexpected_resp,
        .handle_notification = test_sar_client_unexpected_resp,
        .handle_data_packet = test_recv_data_seg_handle_packet
    };
    static const guint8 buf1[] = {
        NCI_MT_DATA_PKT | NCI_PBF | 1, 0,
    };
    static const guint8 buf2[] = {
        1, 0x01, /* End of the first packet */
        NCI_MT_DATA_PKT | NCI_PBF | 1, 0, 1, 0x02
    };
    static const guint8 buf3[] = {
        NCI_MT_DATA_PKT | 1, 0, 1
    };
    static const guint8 buf4[] = {
        0x03 /* End of the complete packet */
    };
    NciSar* sar;
    TestHalIo* test_io = test_hal_io_new();
    TestRecvDataSeg test;

    memset(&test, 0, sizeof(test));
    test.client.fn = &test_recv_data_seg_fn;

    sar = nci_sar_new(&test_io->io, &test.client);
    g_assert(nci_sar_start(sar));
    g_assert(test_io->sar);

    nci_sar_set_max_logical_connections(sar, 2);
    test_io->sar->fn->read(test_io->sar, buf1, sizeof(buf1));
    test_io->sar->fn->read(test_io->sar, buf2, sizeof(buf2));
    test_io->sar->fn->read(test_io->sar, buf3, sizeof(buf3));
    test_io->sar->fn->read(test_io->sar, buf4, sizeof(buf4));
    g_assert(test.packet_received);
    nci_sar_set_max_logical_connections(sar, 1);

    nci_sar_free(sar);
    test_hal_io_free(test_io);
}

/*==========================================================================*
 * recv_reset
 *==========================================================================*/

static
void
test_reset(
    void)
{
    static const NciSarClientFunctions test_reset_fn = {
        .error = test_sar_client_unexpected,
        .handle_response = test_sar_client_unexpected_resp,
        .handle_notification = test_sar_client_unexpected_resp,
        .handle_data_packet = test_sar_client_unexpected_data_packet
    };
    static const guint8 buf1[] = {
        NCI_MT_DATA_PKT | NCI_PBF | 1, 0, 1, 0x02
    };
    NciSar* sar;
    TestHalIo* test_io = test_hal_io_new();
    NciSarClient client;

    memset(&client, 0, sizeof(client));
    client.fn = &test_reset_fn;

    sar = nci_sar_new(&test_io->io, &client);
    g_assert(nci_sar_start(sar));
    g_assert(test_io->sar);

    nci_sar_set_max_logical_connections(sar, 2);
    test_io->sar->fn->read(test_io->sar, buf1, sizeof(buf1));
    g_assert(nci_sar_send_command(sar, TEST_GID, TEST_OID, NULL,
        test_client_unexpected_completion, NULL, NULL));
    nci_sar_reset(sar);

    nci_sar_cancel(sar, nci_sar_send_command(sar, TEST_GID, TEST_OID, NULL,
        test_client_unexpected_completion, NULL, NULL));
    nci_sar_reset(sar);

    g_assert(nci_sar_send_command(sar, TEST_GID, TEST_OID, NULL,
        test_client_unexpected_completion, NULL, NULL));
    nci_sar_cancel(sar, nci_sar_send_command(sar, TEST_GID, TEST_OID, NULL,
        test_client_unexpected_completion, NULL, NULL));
    nci_sar_reset(sar);

    nci_sar_free(sar);
    test_hal_io_free(test_io);
}

/*==========================================================================*
 * Common
 *==========================================================================*/

#define TEST_(name) "/nci_sar/" name

int main(int argc, char* argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func(TEST_("null"), test_null);
    g_test_add_func(TEST_("fail"), test_fail);
    g_test_add_func(TEST_("basic"), test_basic);
    g_test_add_func(TEST_("send_seg"), test_send_seg);
    g_test_add_func(TEST_("send_err"), test_send_err);
    g_test_add_func(TEST_("recv_ntf"), test_recv_ntf);
    g_test_add_func(TEST_("recv_ntf_data"), test_recv_ntf_data);
    g_test_add_func(TEST_("recv_multi"), test_recv_multi);
    g_test_add_func(TEST_("recv_seg"), test_recv_seg);
    g_test_add_func(TEST_("recv_seg_err"), test_recv_seg_err);
    g_test_add_func(TEST_("bad_cid"), test_bad_cid);
    g_test_add_func(TEST_("recv_data"), test_recv_data);
    g_test_add_func(TEST_("recv_data_seg"), test_recv_data_seg);
    g_test_add_func(TEST_("recv_reset"), test_reset);
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
