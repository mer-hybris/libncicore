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

#include "nci_sm.h"
#include "nci_sar.h"
#include "nci_param.h"
#include "nci_state_p.h"
#include "nci_transition.h"
#include "nci_log.h"

#include <gutil_idlepool.h>
#include <gutil_macros.h>
#include <gutil_misc.h>

#include <glib-object.h>

typedef GObjectClass NciSmObjectClass;

typedef struct nci_sm_closure {
    GCClosure cclosure;
    NciSmFunc func;
    gpointer* user_data;
} NciSmClosure;

typedef struct nci_sm_intf_activated_closure {
    GCClosure cclosure;
    NciSmIntfActivationFunc func;
    gpointer* user_data;
} NciSmIntfActivationClosure;

typedef struct nci_sm_object {
    GObject object;
    NciSm sm;
    GUtilIdlePool* pool;
    GPtrArray* states;
    GPtrArray* transitions;
    NciTransition* reset_transition;
    NciTransition* next_transition;
    NciTransition* active_transition;
    NciState* active_state;
    guint32 pending_signals;
} NciSmObject;

G_DEFINE_TYPE(NciSmObject, nci_sm_object, G_TYPE_OBJECT)
#define NCI_TYPE_SM (nci_sm_object_get_type())
#define NCI_SM(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        NCI_TYPE_SM, NciSmObject))
#define NCI_SM_GET_CLASS(obj)  G_TYPE_INSTANCE_GET_CLASS((obj), \
        NCI_TYPE_SM, NciSmObjectClass)

typedef enum nci_sm_signal {
    SIGNAL_NEXT_STATE,
    SIGNAL_LAST_STATE,
    SIGNAL_INTF_ACTIVATED,
    SIGNAL_COUNT
} NCI_SM_SIGNAL;

#define SIGNAL_LAST_STATE_NAME      "nci-sm-last-state"
#define SIGNAL_NEXT_STATE_NAME      "nci-sm-next-state"
#define SIGNAL_INTF_ACTIVATED_NAME  "nci-sm-intf-activated"

static guint nci_sm_signals[SIGNAL_COUNT] = { 0 };

#define NCI_IS_INTERNAL_STATE(state) ((state) < NCI_RFST_IDLE)

static inline NciSmObject* nci_sm_object(NciSm* sm) /* NULL safe */
    { return G_LIKELY(sm) ? NCI_SM(G_CAST(sm, NciSmObject, sm)) : NULL; }

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
inline
void
nci_sm_queue_signal(
    NciSmObject* self,
    NCI_SM_SIGNAL sig)
{
    self->pending_signals |= (1 << sig);
}

static
void
nci_sm_emit_pending_signals(
    NciSmObject* self)
{
    if (self->pending_signals) {
        GObject* obj = &self->object;
        int sig;

        /* Handlers could drops their references to us */
        g_object_ref(obj);
        for (sig = 0; self->pending_signals && sig < SIGNAL_COUNT; sig++) {
            const guint32 signal_bit = (1 << sig);
            if (self->pending_signals & signal_bit) {
                self->pending_signals &= ~signal_bit;
                g_signal_emit(obj, nci_sm_signals[sig], 0);
            }
        }
        /* And release the temporary reference */
        g_object_unref(obj);
    }
}

static
NciState*
nci_sm_add_new_state(
    NciSm* sm,
    NciState* (*fn)(NciSm* sm))
{
    NciState* state = fn(sm);

    nci_sm_add_state(sm, state);
    nci_state_unref(state);
    return state;
}

static
void
nci_sm_add_new_transition(
    NciSm* sm,
    NCI_STATE state,
    NciTransition* (*fn)(NciSm* sm))
{
    NciTransition* transition = fn(sm);

    nci_sm_add_transition(sm, state, transition);
    nci_transition_unref(transition);
}

static
void
nci_sm_weak_pointer_notify(
   gpointer data,
   GObject* dead_object)
{
    NciSm** ptr = data;
    NciSmObject* dead_self = G_CAST(dead_object, NciSmObject, object);

    if (*ptr == &dead_self->sm) {
        *ptr = NULL;
    }
}

static
void
nci_sm_finish_active_transition(
    NciSmObject* self)
{
    if (self->active_transition) {
        NciTransition* finished = self->active_transition;

        self->active_transition = NULL;
        nci_transition_finished(finished);
        nci_transition_unref(finished);
    }
}

static
gboolean
nci_sm_start_transition(
    NciSmObject* self,
    NciTransition* transition)
{
    /* Caller checks that transition is not NULL */
    if (self->active_transition != transition) {
        nci_sm_finish_active_transition(self);
    }

    /* Optimistically mark this transition as active */
    self->active_transition = nci_transition_ref(transition);
    if (nci_transition_start(transition)) {
        /* Transition has started, deactivate the state */
        if (self->active_state) {
            nci_state_leave(self->active_state);
            self->active_state = NULL;
        }
        return TRUE;
    } else if (self->active_transition == transition) {
        /* Ne need to finish it because it hasn't beenstarted */
        nci_transition_unref(self->active_transition);
        self->active_transition = NULL;
    }
    return FALSE;
}

static
void
nci_sm_set_last_state(
    NciSmObject* self,
    NciState* state)
{
    NciSm* sm = &self->sm;

    if (sm->last_state != state && state) {
        GDEBUG("Current state %s -> %s", sm->last_state->name, state->name);
        sm->last_state = state;
        nci_sm_queue_signal(self, SIGNAL_LAST_STATE);
    }
}

static
void
nci_sm_set_next_state(
    NciSmObject* self,
    NciState* state)
{
    NciSm* sm = &self->sm;

    if (sm->next_state != state && state) {
        GDEBUG("Next state %s -> %s", sm->next_state->name, state->name);
        sm->next_state = state;
        nci_sm_queue_signal(self, SIGNAL_NEXT_STATE);
    }
}

static
NciState*
nci_sm_state_by_id(
    NciSmObject* self,
    NCI_STATE id)
{
    const GPtrArray* states = self->states;
    const guint index = (guint)id;

    if (index < states->len) {
        gpointer state = states->pdata[index];

        if (state) {
            return NCI_STATE(state);
        }
    }
    GWARN("Unknown state %d", id);
    return NULL;
}

static
void
nci_sm_stall_internal(
    NciSmObject* self,
    NCI_STALL type)
{
    NciSmIo* io = self->sm.io;
    NciState* state = nci_sm_state_by_id(self, (type == NCI_STALL_STOP) ?
            NCI_STATE_STOP : NCI_STATE_ERROR);

    if (G_LIKELY(io)) {
        io->cancel(io);
    }
    nci_sm_finish_active_transition(self);
    nci_sm_set_last_state(self, state);
    nci_sm_set_next_state(self, state);
    if (self->active_state != state) {
        if (self->active_state) {
            nci_state_leave(self->active_state);
        }
        self->active_state = state;
        nci_state_enter(state, NULL);
    }
    nci_sm_emit_pending_signals(self);
}

static
void
nci_sm_enter_state_internal(
    NciSmObject* self,
    NciState* state,
    NciParam* param)
{
    NciTransition* next_transition;

    /* Entering any state terminates the transition */
    nci_sm_finish_active_transition(self);

    /* Activate the new state */
    if (state == self->active_state) {
        nci_state_reenter(state, param);
    } else {
        if (self->active_state) {
            nci_state_leave(self->active_state);
        }
        self->active_state = state;
        nci_state_enter(state, param);
    }

    /* Start the next transition if there is any */
    next_transition = self->next_transition;
    self->next_transition = NULL;
    if (next_transition && next_transition->dest == state) {
        nci_transition_unref(next_transition);
        next_transition = NULL;
    }

    if (next_transition) {
        nci_sm_start_transition(self, next_transition);
        nci_transition_unref(next_transition);
    }

    nci_sm_set_last_state(self, state);
    if (!self->active_transition) {
        nci_sm_set_next_state(self, state);
    }
    nci_sm_emit_pending_signals(self);
}

/*
 * We can't directly connect the provided callback because it expects
 * the first parameter to point to NciSm part of NciSmObject but glib
 * will invoke it with NciSmObject pointer as the first parameter. We
 * need to replace the source.
 */

static
void
nci_sm_closure_cb(
    NciSmObject* self,
    NciSmClosure* closure)
{
    closure->func(&self->sm, closure->user_data);
}

static
void
nci_sm_intf_activated_closure_cb(
    NciSmObject* self,
    const NciIntfActivationNtf* ntf,
    NciSmIntfActivationClosure* closure)
{
    closure->func(&self->sm, ntf, closure->user_data);
}

static
gulong
nci_sm_add_signal_handler(
    NciSm* sm,
    NCI_SM_SIGNAL signal,
    NciSmFunc func,
    void* user_data)
{
    NciSmObject* self = nci_sm_object(sm);

    if (G_LIKELY(self) && G_LIKELY(func)) {
        NciSmClosure* closure = (NciSmClosure*)
            g_closure_new_simple(sizeof(NciSmClosure), NULL);

        closure->cclosure.closure.data = closure;
        closure->cclosure.callback = G_CALLBACK(nci_sm_closure_cb);
        closure->func = func;
        closure->user_data = user_data;

        return g_signal_connect_closure_by_id(self, nci_sm_signals[signal], 0,
            &closure->cclosure.closure, FALSE);
    }
    return 0;
}

/*==========================================================================*
 * Interface
 *==========================================================================*/

NciState*
nci_sm_enter_state(
    NciSm* sm,
    NCI_STATE id,
    NciParam* param)
{
    NciSmObject* self = nci_sm_object(sm);

    if (G_LIKELY(self)) {
        const GPtrArray* states = self->states;
        const guint index = (guint)id;

        if (index < states->len) {
            gpointer state_ptr = states->pdata[index];

            if (state_ptr) {
                NciState* state = NCI_STATE(state_ptr);

                nci_param_ref(param);
                nci_sm_enter_state_internal(self, state, param);
                nci_param_unref(param);
                return state;
            }
        }
        GERR("Unknown state %d", id);
    }
    return NULL;
}

void
nci_sm_switch_to(
    NciSm* sm,
    NCI_STATE id)
{
    NciState* next = nci_sm_get_state(sm, id);

    if (G_LIKELY(next) && sm->next_state != next) {
        NciSmObject* self = nci_sm_object(sm);

        if (self->next_transition) {
            nci_transition_unref(self->next_transition);
            self->next_transition = NULL;
        }
        if (self->active_transition) {
            NciState* dest = self->active_transition->dest;

            if (dest != next) {
                self->next_transition = nci_state_get_transition(dest, id);
                if (self->next_transition) {
                    nci_transition_ref(self->next_transition);
                    nci_sm_set_next_state(self, next);
                } else if (NCI_IS_INTERNAL_STATE(id)) {
                    /* Internal states are entered without transition
                     * and take no parameters */
                    nci_sm_enter_state_internal(self, next, NULL);
                } else {
                    GERR("No transition %s -> %s", dest->name, next->name);
                    nci_sm_stall_internal(self, NCI_STALL_ERROR);
                }
            }
        } else {
            NciTransition* next_transition =
                nci_state_get_transition(sm->last_state, id);

            if (next_transition) {
                if (nci_sm_start_transition(self, next_transition)) {
                    nci_sm_set_next_state(self, next);
                } else {
                    nci_sm_stall_internal(self, NCI_STALL_ERROR);
                }
            } else if (NCI_IS_INTERNAL_STATE(id)) {
                /* Internal states are entered without transition and
                 * take no parameters. */
                nci_sm_enter_state_internal(self, next, NULL);
            } else {
                NciState* idle = nci_sm_state_by_id(self, NCI_RFST_IDLE);

                /* Switch to idle state first */
                if (nci_sm_start_transition(self, self->reset_transition)) {
                    if (id == NCI_RFST_IDLE) {
                        nci_sm_set_next_state(self, idle);
                    } else {
                        self->next_transition =
                            nci_state_get_transition(idle, id);
                        if (self->next_transition) {
                            nci_transition_ref(self->next_transition);
                            nci_sm_set_next_state(self, next);
                        } else {
                            GERR("No transition %s -> %s", idle->name,
                                next->name);
                            nci_sm_stall_internal(self, NCI_STALL_ERROR);
                        }
                    }
                } else {
                    nci_sm_stall_internal(self, NCI_STALL_ERROR);
                }
            }
        }
        nci_sm_emit_pending_signals(self);
    }
}

void
nci_sm_stall(
    NciSm* sm,
    NCI_STALL type)
{
    NciSmObject* self = nci_sm_object(sm);

    if (G_LIKELY(self)) {
        nci_sm_stall_internal(self, type);
    }
}

/*==========================================================================*
 * Interface for NciCore
 *==========================================================================*/

NciSm*
nci_sm_new(
    NciSmIo* io)
{
    NciSmObject* self = g_object_new(NCI_TYPE_SM, NULL);
    NciSm* sm = &self->sm;
    NciTransition* deactivate_to_idle;

    sm->io = io;

    /* Default setup */
    nci_sm_add_new_state(sm, nci_state_idle_new);
    nci_sm_add_new_state(sm, nci_state_discovery_new);
    nci_sm_add_new_state(sm, nci_state_poll_active_new);
    nci_sm_add_new_state(sm, nci_state_w4_all_discoveries_new);
    nci_sm_add_new_state(sm, nci_state_w4_host_select_new);

    /*
     * Reset transition could be added to the internal states, i.e.
     * NCI_STATE_INIT, NCI_STATE_ERROR and NCI_STATE_STOP but it's
     * not really necessary. If the last state doesn't know where
     * to go, reset transition is applied anyway and state machine
     * then happily continues from NCI_RFST_IDLE state. So the end
     * result would be the same as if we didn't add any transitions
     * to the internal states.
     */
    self->reset_transition = nci_transition_reset_new(sm);

    /* Set up the transitions */
    deactivate_to_idle = nci_transition_deactivate_to_idle_new(sm);
    nci_sm_add_transition(sm, NCI_RFST_DISCOVERY, deactivate_to_idle);
    nci_sm_add_transition(sm, NCI_RFST_W4_ALL_DISCOVERIES, deactivate_to_idle);
    nci_sm_add_transition(sm, NCI_RFST_W4_HOST_SELECT, deactivate_to_idle);
    nci_transition_unref(deactivate_to_idle);

    nci_sm_add_new_transition(sm,
        NCI_RFST_IDLE, nci_transition_idle_to_discovery_new);
    nci_sm_add_new_transition(sm,
        NCI_RFST_POLL_ACTIVE, nci_transition_poll_active_to_discovery_new);
    nci_sm_add_new_transition(sm,
        NCI_RFST_POLL_ACTIVE, nci_transition_poll_active_to_idle_new);
    return sm;
}

void
nci_sm_free(
    NciSm* sm)
{
    NciSmObject* self = nci_sm_object(sm);

    if (G_LIKELY(self)) {
        g_object_unref(self);
    }
}

void
nci_sm_handle_ntf(
    NciSm* sm,
    guint8 gid,
    guint8 oid,
    const GUtilData* data)
{
    NciSmObject* self = nci_sm_object(sm);

    if (self) {
        if (self->active_transition) {
            nci_transition_handle_ntf(self->active_transition, gid, oid, data);
        } else {
            GASSERT(sm->last_state);
            nci_state_handle_ntf(sm->last_state, gid, oid, data);
        }
    }
}

void
nci_sm_add_state(
    NciSm* sm,
    NciState* state)
{
    NciSmObject* self = nci_sm_object(sm);

    if (G_LIKELY(self) && G_LIKELY(state)) {
        GPtrArray* states = self->states;

        if (states->len <= state->state) {
            g_ptr_array_set_size(states, state->state + 1);
        }
        if (states->pdata[state->state] != state) {
            nci_state_unref(states->pdata[state->state]);
            states->pdata[state->state] = nci_state_ref(state);
        }
    }
}

void
nci_sm_add_transition(
    NciSm* sm,
    NCI_STATE state,
    NciTransition* transition)
{
    NciSmObject* self = nci_sm_object(sm);

    if (G_LIKELY(self) && G_LIKELY(transition)) {
        NciState* src = nci_sm_state_by_id(self, state);

        if (G_LIKELY(src)) {
            g_ptr_array_add(self->transitions, nci_transition_ref(transition));
            nci_state_add_transition(src, transition);
        }
    }
}

gulong
nci_sm_add_last_state_handler(
    NciSm* sm,
    NciSmFunc func,
    void* user_data)
{
    return nci_sm_add_signal_handler(sm, SIGNAL_LAST_STATE, func, user_data);
}

gulong
nci_sm_add_next_state_handler(
    NciSm* sm,
    NciSmFunc func,
    void* user_data)
{
    return nci_sm_add_signal_handler(sm, SIGNAL_NEXT_STATE, func, user_data);
}

gulong
nci_sm_add_intf_activated_handler(
    NciSm* sm,
    NciSmIntfActivationFunc func,
    void* user_data)
{
    NciSmObject* self = nci_sm_object(sm);

    if (G_LIKELY(self) && G_LIKELY(func)) {
        NciSmIntfActivationClosure* closure = (NciSmIntfActivationClosure*)
            g_closure_new_simple(sizeof(NciSmIntfActivationClosure), NULL);
        GCClosure* cclosure = &closure->cclosure;

        cclosure->closure.data = closure;
        cclosure->callback = G_CALLBACK(nci_sm_intf_activated_closure_cb);
        closure->func = func;
        closure->user_data = user_data;

        return g_signal_connect_closure_by_id(self, nci_sm_signals
            [SIGNAL_INTF_ACTIVATED], 0, &cclosure->closure, FALSE);
    }
    return 0;
}

void
nci_sm_remove_handler(
    NciSm* sm,
    gulong id)
{
    if (G_LIKELY(id)) {
        NciSmObject* self = nci_sm_object(sm);

        if (G_LIKELY(self)) {
            g_signal_handler_disconnect(self, id);
        }
    }
}

void
nci_sm_remove_handlers(
    NciSm* sm,
    gulong* ids,
    guint count)
{
    NciSmObject* self = nci_sm_object(sm);

    if (G_LIKELY(self)) {
        gutil_disconnect_handlers(self, ids, count);
    }
}

/*==========================================================================*
 * Interface for states and transitions
 *==========================================================================*/

NciSar*
nci_sm_sar(
    NciSm* sm)
{
    return (G_LIKELY(sm) && G_LIKELY(sm->io)) ? sm->io->sar : NULL;
}

void
nci_sm_add_weak_pointer(
    NciSm** ptr)
{
    if (G_LIKELY(ptr)) {
        NciSmObject* self = nci_sm_object(*ptr);

        if (self) {
            g_object_weak_ref(&self->object, nci_sm_weak_pointer_notify, ptr);
        }
    }
}

void
nci_sm_remove_weak_pointer(
    NciSm** ptr)
{
    if (G_LIKELY(ptr)) {
        NciSmObject* self = nci_sm_object(*ptr);

        if (self) {
            g_object_weak_unref(&self->object, nci_sm_weak_pointer_notify, ptr);
            *ptr = NULL;
        }
    }
}

gboolean
nci_sm_supports_protocol(
    NciSm* sm,
    NCI_PROTOCOL protocol)
{
    switch (protocol) {
    case NCI_PROTOCOL_T2T:
    case NCI_PROTOCOL_ISO_DEP:
        return TRUE;
    case NCI_PROTOCOL_UNDETERMINED:
    case NCI_PROTOCOL_T1T:
    case NCI_PROTOCOL_T3T:
    case NCI_PROTOCOL_NFC_DEP:
        break;
    }
    return FALSE;
}

gboolean
nci_sm_active_transition(
    NciSm* sm,
    NciTransition* transition)
{
    NciSmObject* self = nci_sm_object(sm);

    return G_LIKELY(self) && G_LIKELY(transition) &&
        self->active_transition == transition;
}

NciState*
nci_sm_get_state(
    NciSm* sm,
    NCI_STATE id)
{
    NciSmObject* self = nci_sm_object(sm);

    return G_LIKELY(self) ? nci_sm_state_by_id(self, id) : NULL;
}

gboolean
nci_sm_send_command(
    NciSm* sm,
    guint8 gid,
    guint8 oid,
    GBytes* payload,
    NciSmResponseFunc resp,
    gpointer user_data)
{
    if (G_LIKELY(sm)) {
        NciSmIo* io = sm->io;

        if (G_LIKELY(io)) {
            return io->send(io, gid, oid, payload, resp, user_data);
        }
    }
    return FALSE;
}

gboolean
nci_sm_send_command_static(
    NciSm* sm,
    guint8 gid,
    guint8 oid,
    const void* payload,
    gsize payload_len,
    NciSmResponseFunc resp,
    gpointer user_data)
{
    gboolean ok = FALSE;

    if (G_LIKELY(sm)) {
        NciSmIo* io = sm->io;

        if (G_LIKELY(io)) {
            GBytes* bytes = g_bytes_new_static(payload, payload_len);

            ok = io->send(io, gid, oid, bytes, resp, user_data);
            g_bytes_unref(bytes);
        }
    }
    return ok;
}

void
nci_sm_intf_activated(
    NciSm* sm,
    const NciIntfActivationNtf* ntf)
{
    NciSmObject* self = nci_sm_object(sm);

    if (G_LIKELY(self)) {
        nci_sar_set_initial_credits(nci_sm_sar(sm), NCI_STATIC_RF_CONN_ID,
            ntf->num_credits);
        g_signal_emit(self, nci_sm_signals[SIGNAL_INTF_ACTIVATED], 0, ntf);
    }
}

/*==========================================================================*
 * Notification handlers
 *==========================================================================*/

void
nci_sm_handle_conn_credits_ntf(
    NciSm* sm,
    const GUtilData* payload)
{
    NciSar* sar = (sm && sm->io) ? sm->io->sar : NULL;

    if (sar) {
        /*
         * Table 17: Control Messages for Connection Credit Management
         *
         * +=========================================================+
         * | Offset | Size | Description                             |
         * +=========================================================+
         * | 0      | 1    | Number of Entries (n)                   |
         * | 1      | 2*n  | Entries                                 |
         * |        |      +-----------------------------------------+
         * |        |      | Size | Description                      |
         * |        |      +-----------------------------------------+
         * |        |      |  1   | Conn ID                          |
         * |        |      |  1   | Credits                          |
         * +=========================================================+
         */
        if (payload->size > 0) {
            const guint n = payload->bytes[0];

            if (payload->size >= (1 + 2 * n)) {
                const guint8* entry = payload->bytes + 1;
                guint i;

                GDEBUG("CORE_CONN_CREDITS_NTF");
                for (i = 0; i < n; i++, entry += 2) {
                    nci_sar_add_credits(sar, entry[0], entry[1]);
                }
                return;
            }
        }
        GWARN("Failed to parse CORE_CONN_CREDITS_NTF");
        nci_sm_stall(sm, NCI_STALL_ERROR);
    }
}

void
nci_sm_handle_rf_deactivate_ntf(
    NciSm* sm,
    const GUtilData* payload)
{
    NciSar* sar = (sm && sm->io) ? sm->io->sar : NULL;

    if (sar) {
        /*
         * Table 62: Control Messages for RF Interface Deactivation
         *
         * RF_DEACTIVATE_NTF
         *
         * +=========================================================+
         * | Offset | Size | Description                             |
         * +=========================================================+
         * | 0      | 1    | Deactivation Type                       |
         * | 1      | 1    | Deactivation Reason                     |
         * +=========================================================+
         */
        if (payload->size >= 2) {
            const guint8 type = payload->bytes[0];

            switch (type) {
            case NCI_DEACTIVATE_TYPE_IDLE:
                GDEBUG("RF_DEACTIVATE_NTF Idle (%u)", payload->bytes[1]);
                nci_sm_enter_state(sm, NCI_RFST_IDLE, NULL);
                return;
            case NCI_DEACTIVATE_TYPE_DISCOVERY:
                GDEBUG("RF_DEACTIVATE_NTF Discovery (%u)", payload->bytes[1]);
                nci_sm_enter_state(sm, NCI_RFST_DISCOVERY, NULL);
                return;
            default:
                GDEBUG("RF_DEACTIVATE_NTF %u (%u)", type, payload->bytes[1]);
                break;
            }
        }
        GWARN("Failed to parse CORE_CONN_CREDITS_NTF");
        nci_sm_stall(sm, NCI_STALL_ERROR);
    }
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
void
nci_sm_object_init(
    NciSmObject* self)
{
    NciSm* sm = &self->sm;

    self->pool = gutil_idle_pool_new();
    self->transitions = g_ptr_array_new_with_free_func((GDestroyNotify)
        nci_transition_unref);
    self->states = g_ptr_array_new_full(NCI_CORE_STATES, (GDestroyNotify)
        nci_state_unref);

    /* Internal states are always there */
    self->active_state = sm->last_state = sm->next_state =
    nci_sm_add_new_state(sm, nci_state_init_new);
    nci_sm_add_new_state(sm, nci_state_error_new);
    nci_sm_add_new_state(sm, nci_state_stop_new);

    /* Enter the initial state */
    nci_state_enter(self->active_state, NULL);
}

static
void
nci_sm_object_finalize(
    GObject* object)
{
    NciSmObject* self = NCI_SM(object);
    NciSm* sm = &self->sm;

    nci_sm_finish_active_transition(self);
    nci_state_leave(self->active_state);
    if (sm->rf_interfaces) {
        g_bytes_unref(sm->rf_interfaces);
    }
    nci_transition_unref(self->next_transition);
    g_ptr_array_free(self->transitions, TRUE);
    g_ptr_array_free(self->states, TRUE);
    nci_transition_unref(self->reset_transition);
    gutil_idle_pool_destroy(self->pool);
    G_OBJECT_CLASS(nci_sm_object_parent_class)->finalize(object);
}

static
void
nci_sm_object_class_init(
    NciSmObjectClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = nci_sm_object_finalize;
    nci_sm_signals[SIGNAL_LAST_STATE] =
        g_signal_new(SIGNAL_LAST_STATE_NAME, G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
    nci_sm_signals[SIGNAL_NEXT_STATE] =
        g_signal_new(SIGNAL_NEXT_STATE_NAME, G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
    nci_sm_signals[SIGNAL_INTF_ACTIVATED] =
        g_signal_new(SIGNAL_INTF_ACTIVATED_NAME, G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1,
            G_TYPE_POINTER);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
