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

#include "nci_core.h"
#include "nci_sar.h"
#include "nci_sm.h"
#include "nci_state.h"
#include "nci_log.h"

#include <gutil_misc.h>
#include <gutil_macros.h>

#include <glib-object.h>

GLOG_MODULE_DEFINE("nci");

typedef struct nci_core_closure {
    GCClosure cclosure;
    NciCoreFunc func;
    gpointer* user_data;
} NciCoreClosure;

typedef struct nci_core_intf_activated_closure {
    GCClosure cclosure;
    NciCoreIntfActivationFunc func;
    gpointer* user_data;
} NciCoreIntfActivationClosure;

typedef struct nci_core_data_packet_closure {
    GCClosure cclosure;
    NciCoreDataPacketFunc func;
    gpointer* user_data;
} NciCoreDataPacketClosure;

typedef struct nci_core_send_data {
    NciCoreSendFunc complete;
    GDestroyNotify destroy;
    gpointer user_data;
} NciCoreSendData;

enum nci_core_events {
    EVENT_LAST_STATE,
    EVENT_NEXT_STATE,
    EVENT_INTF_ACTIVATED,
    EVENT_COUNT
};

typedef struct nci_core_object {
    GObject object;
    NciCore core;
    NciSarClient sar_client;
    NciSmIo io;
    NciSm* sm;
    guint cmd_id;
    guint cmd_timeout_id;
    guint8 rsp_gid;
    guint8 rsp_oid;
    NciSmResponseFunc rsp_handler;
    gpointer rsp_data;
    gulong event_ids[EVENT_COUNT];
} NciCoreObject;

typedef GObjectClass NciCoreObjectClass;
G_DEFINE_TYPE(NciCoreObject, nci_core_object, G_TYPE_OBJECT)
#define NCI_TYPE_CORE (nci_core_object_get_type())
#define NCI_CORE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        NCI_TYPE_CORE, NciCoreObject))

typedef enum nci_core_signal {
    SIGNAL_CURRENT_STATE,
    SIGNAL_NEXT_STATE,
    SIGNAL_INTF_ACTIVATED,
    SIGNAL_DATA_PACKET,
    SIGNAL_COUNT
} NCI_CORE_SIGNAL;

#define SIGNAL_CURRENT_STATE_NAME   "nci-core-current-state"
#define SIGNAL_NEXT_STATE_NAME      "nci-core-next-state"
#define SIGNAL_INTF_ACTIVATED_NAME  "nci-core-intf-activated"
#define SIGNAL_DATA_PACKET_NAME     "nci-core-data-packet"

static guint nci_core_signals[SIGNAL_COUNT] = { 0 };

#define DEFAULT_TIMEOUT (2000) /* msec */

static inline NciCoreObject* nci_core_object(NciCore* ptr)
{ return G_LIKELY(ptr) ? /* This one should be NULL safe */
             NCI_CORE(G_CAST(ptr, NciCoreObject, core)) : NULL; }

static inline NciCoreObject* nci_core_object_from_sar_client(NciSarClient* ptr)
    { return NCI_CORE(G_CAST(ptr, NciCoreObject, sar_client)); }

static inline NciCoreObject* nci_core_object_from_sm_io(NciSmIo* ptr)
    { return NCI_CORE(G_CAST(ptr, NciCoreObject, io)); }

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
void
nci_core_cancel_command(
    NciCoreObject* self)
{
    if (self->cmd_id) {
        gpointer user_data = self->rsp_data;

        nci_sar_cancel(self->io.sar, self->cmd_id);
        self->cmd_id = 0;
        self->rsp_data = NULL;
        if (self->cmd_timeout_id) {
            g_source_remove(self->cmd_timeout_id);
            self->cmd_timeout_id = 0;
        }
        if (self->rsp_handler) {
            NciSmResponseFunc handler = self->rsp_handler;
            GUtilData payload;

            self->rsp_handler = NULL;
            memset(&payload, 0, sizeof(payload));
            handler(NCI_REQUEST_CANCELLED, &payload, user_data);
        }
    }
}

static
void
nci_core_command_completion(
    NciSarClient* sar_client,
    gboolean success,
    gpointer user_data)
{
    if (!success) {
        NciCoreObject* self = NCI_CORE(user_data);

        GWARN("Failed to send command %02x/%02x", self->rsp_gid, self->rsp_oid);
        nci_sm_stall(self->sm, NCI_STALL_ERROR);
    }
}

static
void
nci_core_io_dummy_resp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    gpointer user_data)
{
    if (status != NCI_REQUEST_SUCCESS) {
        GWARN("Command %02x/%02x failed", NCI_CORE(user_data)->rsp_gid,
            NCI_CORE(user_data)->rsp_oid);
    }
}

static
gboolean
nci_core_command_timeout(
    gpointer user_data)
{
    NciCoreObject* self = NCI_CORE(user_data);
    gpointer rsp_data = self->rsp_data;

    GWARN("Command %02x/%02x timed out", self->rsp_gid, self->rsp_oid);
    self->cmd_timeout_id = 0;
    self->rsp_data = NULL;
    nci_sar_cancel(self->io.sar, self->cmd_id);
    self->cmd_id = 0;
    if (self->rsp_handler) {
        NciSmResponseFunc handler = self->rsp_handler;
        GUtilData payload;

        self->rsp_handler = NULL;
        memset(&payload, 0, sizeof(payload));
        handler(NCI_REQUEST_TIMEOUT, &payload, rsp_data);
    }
    nci_sm_stall(self->sm, NCI_STALL_ERROR);
    return G_SOURCE_REMOVE;
}

static
void
nci_core_send_data_msg_complete(
    NciSarClient* client,
    gboolean ok,
    gpointer user_data)
{
    NciCoreSendData* send = user_data;
    NciCoreObject* self = nci_core_object_from_sar_client(client);

    if (send->complete) {
        send->complete(&self->core, ok, send->user_data);
    }
}

static
void
nci_core_send_data_msg_destroy(
    gpointer data)
{
    NciCoreSendData* send = data;

    if (send->destroy) {
        send->destroy(send->user_data);
    }
    g_slice_free(NciCoreSendData, send);
}

static
void
nci_core_last_state_changed(
    NciSm* sm,
    void* user_data)
{
    NciCoreObject* self = NCI_CORE(user_data);

    self->core.current_state = sm->last_state->state;
    g_signal_emit(self, nci_core_signals[SIGNAL_CURRENT_STATE], 0);
}

static
void
nci_core_next_state_changed(
    NciSm* sm,
    void* user_data)
{
    NciCoreObject* self = NCI_CORE(user_data);

    self->core.next_state = sm->next_state->state;
    g_signal_emit(self, nci_core_signals[SIGNAL_NEXT_STATE], 0);
}

static
void
nci_core_intf_activated(
    NciSm* sm,
    const NciIntfActivationNtf* ntf,
    void* user_data)
{
    g_signal_emit(NCI_CORE(user_data), nci_core_signals
        [SIGNAL_INTF_ACTIVATED], 0, ntf);
}

/*
 * We can't directly connect the provided callback because
 * it expects the first parameter to point to NciCore part
 * of NciCoreObject but glib will invoke it with NciSmObject
 * pointer as the first parameter. We need to replace the source.
 */

static
void
nci_core_closure_cb(
    NciCoreObject* self,
    NciCoreClosure* closure)
{
    closure->func(&self->core, closure->user_data);
}

static
void
nci_core_intf_activated_closure_cb(
    NciCoreObject* self,
    const NciIntfActivationNtf* ntf,
    NciCoreIntfActivationClosure* closure)
{
    closure->func(&self->core, ntf, closure->user_data);
}

static
void
nci_core_data_packet_closure_cb(
    NciCoreObject* self,
    guint8 cid,
    const void* payload,
    guint len,
    NciCoreDataPacketClosure* closure)
{
#pragma message("Use GUtilData for payload?")
    closure->func(&self->core, cid, payload, len, closure->user_data);
}

static
gulong
nci_core_add_signal_handler(
    NciCore* core,
    NCI_CORE_SIGNAL signal,
    NciCoreFunc func,
    void* user_data)
{
    NciCoreObject* self = nci_core_object(core);

    if (G_LIKELY(self) && G_LIKELY(func)) {
        NciCoreClosure* closure = (NciCoreClosure*)
            g_closure_new_simple(sizeof(NciCoreClosure), NULL);

        closure->cclosure.closure.data = closure;
        closure->cclosure.callback = G_CALLBACK(nci_core_closure_cb);
        closure->func = func;
        closure->user_data = user_data;

        return g_signal_connect_closure_by_id(self, nci_core_signals[signal],
            0, &closure->cclosure.closure, FALSE);
    }
    return 0;
}

/*==========================================================================*
 * NciSmIo callbacks
 *==========================================================================*/

static
void
nci_core_io_cancel(
    NciSmIo* io)
{
    NciCoreObject* self = nci_core_object_from_sm_io(io);

    self->rsp_handler = NULL;
    self->rsp_data = NULL;
    nci_core_cancel_command(self);
}

static
gboolean
nci_core_io_send(
    NciSmIo* io,
    guint8 gid,
    guint8 oid,
    GBytes* payload,
    NciSmResponseFunc resp,
    gpointer user_data)
{
    NciCoreObject* self = nci_core_object_from_sm_io(io);
    NciCore* core = &self->core;

    /* Cancel the previous one, if any */
    nci_core_cancel_command(self);

    self->rsp_gid = gid;
    self->rsp_oid = oid;
    if (resp) {
        self->rsp_handler = resp;
        self->rsp_data = user_data;
    } else {
        self->rsp_handler = nci_core_io_dummy_resp;
        self->rsp_data = self;
    }
    self->cmd_id = nci_sar_send_command(self->io.sar, gid, oid, payload,
        nci_core_command_completion, NULL, self);
    if (self->cmd_id) {
        if (core->cmd_timeout) {
            self->cmd_timeout_id = g_timeout_add(core->cmd_timeout,
                nci_core_command_timeout, self);
        }
        return TRUE;
    } else {
        self->rsp_handler = NULL;
        self->rsp_data = NULL;
        return FALSE;
    }
}

/*==========================================================================*
 * SAR client
 *==========================================================================*/

static
void
nci_core_sar_error(
    NciSarClient* client)
{
    GWARN("State machine broke");
    nci_sm_stall(nci_core_object_from_sar_client(client)->sm, NCI_STALL_ERROR);
}

static
void
nci_core_sar_handle_response(
    NciSarClient* client,
    guint8 gid,
    guint8 oid,
    const void* data,
    guint len)
{
    NciCoreObject* self = nci_core_object_from_sar_client(client);

    if (self->rsp_handler) {
        if (self->rsp_gid == gid && self->rsp_oid == oid) {
            NciSmResponseFunc handler = self->rsp_handler;
            gpointer handler_data = self->rsp_data;
            GUtilData payload;

            if (self->cmd_timeout_id) {
                g_source_remove(self->cmd_timeout_id);
                self->cmd_timeout_id = 0;
            }

            self->cmd_id = 0;
            self->rsp_handler = NULL;
            self->rsp_data = NULL;

            payload.bytes = data;
            payload.size = len;
            handler(NCI_REQUEST_SUCCESS, &payload, handler_data);
        } else {
            GWARN("Invalid response %02x/%02x", gid, oid);
        }
    } else {
        GWARN("Unexpected response %02x/%02x", gid, oid);
    }
}

static
void
nci_core_sar_handle_notification(
    NciSarClient* client,
    guint8 gid,
    guint8 oid,
    const void* payload_bytes,
    guint payload_len)
{
    NciCoreObject* self = nci_core_object_from_sar_client(client);
    GUtilData payload;

    payload.bytes = payload_bytes;
    payload.size = payload_len;
    nci_sm_handle_ntf(self->sm, gid, oid, &payload);
}

static
void
nci_core_sar_handle_data_packet(
    NciSarClient* client,
    guint8 cid,
    const void* payload,
    guint len)
{
    g_signal_emit(nci_core_object_from_sar_client(client), nci_core_signals
        [SIGNAL_DATA_PACKET], 0, cid, payload, len);
}

/*==========================================================================*
 * Interface
 *==========================================================================*/

NciCore*
nci_core_new(
    NciHalIo* hal)
{
    if (G_LIKELY(hal)) {
        NciCoreObject* self = g_object_new(NCI_TYPE_CORE, NULL);
        NciCore* core = &self->core;
        NciSm* sm;

        self->io.sar = nci_sar_new(hal, &self->sar_client);
        self->sm = sm = nci_sm_new(&self->io);

        core->current_state = sm->last_state->state;
        core->next_state = sm->next_state->state;

        self->event_ids[EVENT_LAST_STATE] = nci_sm_add_last_state_handler(sm,
            nci_core_last_state_changed, self);
        self->event_ids[EVENT_NEXT_STATE] = nci_sm_add_next_state_handler(sm,
            nci_core_next_state_changed, self);
        self->event_ids[EVENT_INTF_ACTIVATED] =
            nci_sm_add_intf_activated_handler(sm,
                nci_core_intf_activated, self);
        return core;
    }
    return NULL;
}

void
nci_core_free(
    NciCore* core)
{
    NciCoreObject* self = nci_core_object(core);

    if (G_LIKELY(self)) {
        NciSm* sm = self->sm;

        sm->io = NULL;
        nci_sar_free(self->io.sar);
        self->io.sar = NULL;
        g_object_unref(self);
    }
}

void
nci_core_restart(
    NciCore* core)
{
    NciCoreObject* self = nci_core_object(core);

    if (G_LIKELY(self)) {
        nci_core_cancel_command(self);
        nci_sar_reset(self->io.sar);
        nci_sm_enter_state(self->sm, NCI_STATE_INIT, NULL);
        nci_sm_switch_to(self->sm, NCI_RFST_IDLE);
    }
}

void
nci_core_set_state(
    NciCore* core,
    NCI_STATE state)
{
    NciCoreObject* self = nci_core_object(core);

    if (G_LIKELY(self)) {
        nci_sm_switch_to(self->sm, state);
    }
}

guint
nci_core_send_data_msg(
    NciCore* core,
    guint8 cid,
    GBytes* payload,
    NciCoreSendFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    NciCoreObject* self = nci_core_object(core);

    if (G_LIKELY(self)) {
        if (complete || destroy) {
            NciCoreSendData* data = g_slice_new0(NciCoreSendData);

            data->complete = complete;
            data->destroy = destroy;
            data->user_data = user_data;
            return nci_sar_send_data_packet(self->io.sar, cid, payload,
                nci_core_send_data_msg_complete,
                nci_core_send_data_msg_destroy, data);
        } else {
            return nci_sar_send_data_packet(self->io.sar, cid, payload,
                NULL, NULL, NULL);
        }
    }
    return 0;
}

void
nci_core_cancel(
    NciCore* core,
    guint id)
{
    NciCoreObject* self = nci_core_object(core);

    if (G_LIKELY(self)) {
        nci_sar_cancel(self->io.sar, id);
    }
}

gulong
nci_core_add_current_state_changed_handler(
    NciCore* core,
    NciCoreFunc func,
    void* data)
{
    return nci_core_add_signal_handler(core, SIGNAL_CURRENT_STATE, func, data);
}

gulong
nci_core_add_next_state_changed_handler(
    NciCore* core,
    NciCoreFunc func,
    void* data)
{
    return nci_core_add_signal_handler(core, SIGNAL_NEXT_STATE, func, data);
}

gulong
nci_core_add_intf_activated_handler(
    NciCore* core,
    NciCoreIntfActivationFunc func,
    void* user_data)
{
    NciCoreObject* self = nci_core_object(core);

    if (G_LIKELY(self) && G_LIKELY(func)) {
        NciCoreIntfActivationClosure* closure = (NciCoreIntfActivationClosure*)
            g_closure_new_simple(sizeof(NciCoreIntfActivationClosure), NULL);
        GCClosure* cclosure = &closure->cclosure;

        cclosure->closure.data = closure;
        cclosure->callback = G_CALLBACK(nci_core_intf_activated_closure_cb);
        closure->func = func;
        closure->user_data = user_data;

        return g_signal_connect_closure_by_id(self, nci_core_signals
            [SIGNAL_INTF_ACTIVATED], 0, &cclosure->closure, FALSE);
    }
    return 0;
}

gulong
nci_core_add_data_packet_handler(
    NciCore* core,
    NciCoreDataPacketFunc func,
    void* user_data)
{
    NciCoreObject* self = nci_core_object(core);

    if (G_LIKELY(self) && G_LIKELY(func)) {
        NciCoreDataPacketClosure* closure = (NciCoreDataPacketClosure*)
            g_closure_new_simple(sizeof(NciCoreDataPacketClosure), NULL);
        GCClosure* cclosure = &closure->cclosure;

        cclosure->closure.data = closure;
        cclosure->callback = G_CALLBACK(nci_core_data_packet_closure_cb);
        closure->func = func;
        closure->user_data = user_data;

        return g_signal_connect_closure_by_id(self, nci_core_signals
            [SIGNAL_DATA_PACKET], 0, &cclosure->closure, FALSE);
    }
    return 0;
}

void
nci_core_remove_handler(
    NciCore* core,
    gulong id)
{
    NciCoreObject* self = nci_core_object(core);

    if (G_LIKELY(self) && G_LIKELY(id)) {
        g_signal_handler_disconnect(self, id);
    }
}

void
nci_core_remove_handlers(
    NciCore* core,
    gulong* ids,
    guint count)
{
    gutil_disconnect_handlers(nci_core_object(core), ids, count);
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
void
nci_core_object_init(
    NciCoreObject* self)
{
    static const NciSarClientFunctions sar_client_functions = {
        .error = nci_core_sar_error,
        .handle_response = nci_core_sar_handle_response,
        .handle_notification = nci_core_sar_handle_notification,
        .handle_data_packet = nci_core_sar_handle_data_packet
    };

    NciCore* core = &self->core;

    core->cmd_timeout = DEFAULT_TIMEOUT;
    self->sar_client.fn = &sar_client_functions;
    self->io.send = nci_core_io_send;
    self->io.cancel = nci_core_io_cancel;
}

static
void
nci_core_object_finalize(
    GObject* object)
{
    NciCoreObject* self = NCI_CORE(object);

    if (self->cmd_timeout_id) {
        g_source_remove(self->cmd_timeout_id);
    }
    nci_sm_remove_all_handlers(self->sm, self->event_ids);
    nci_sm_free(self->sm);
    nci_sar_free(self->io.sar);
    G_OBJECT_CLASS(nci_core_object_parent_class)->finalize(object);
}

static
void
nci_core_object_class_init(
    NciCoreObjectClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = nci_core_object_finalize;
    nci_core_signals[SIGNAL_CURRENT_STATE] =
        g_signal_new(SIGNAL_CURRENT_STATE_NAME, G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
    nci_core_signals[SIGNAL_NEXT_STATE] =
        g_signal_new(SIGNAL_NEXT_STATE_NAME, G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
    nci_core_signals[SIGNAL_INTF_ACTIVATED] =
        g_signal_new(SIGNAL_INTF_ACTIVATED_NAME, G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
            G_TYPE_POINTER);
    nci_core_signals[SIGNAL_DATA_PACKET] =
        g_signal_new(SIGNAL_DATA_PACKET_NAME, G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 3,
            G_TYPE_UCHAR, G_TYPE_POINTER, G_TYPE_UINT);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
