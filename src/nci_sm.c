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

#include "nci_sm.h"
#include "nci_sar.h"
#include "nci_param.h"
#include "nci_state.h"
#include "nci_transition.h"
#include "nci_log.h"

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
    GPtrArray* states;
    GPtrArray* transitions;
    NciTransition* reset_transition;
    NciTransition* next_transition;
    NciTransition* active_transition;
    NciState* active_state;
    gint entering_state;
    guint pending_switch_id;
    guint32 pending_signals;
} NciSmObject;

typedef struct nci_sm_switch {
    NciSmObject* obj;
    NciState* state;
} NciSmSwitch;

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

/* Config file name is extern for unit tests */
const char* nci_sm_config_file = "/etc/libncicore.conf";
static const char CONFIG_SECTION[] = "Configuration";
static const char CONFIG_LIST_SEPARATORS[] = ";,";
static const char CONFIG_ENTRY_TECHNOLOGIES[] = "Technologies";
static const char CONFIG_TECHNOLOGY_TYPE_A[] = "A";
static const char CONFIG_TECHNOLOGY_TYPE_B[] = "B";
static const char CONFIG_TECHNOLOGY_TYPE_F[] = "F";
static const char CONFIG_TECHNOLOGY_TYPE_V[] = "V";
static const char CONFIG_TECHNOLOGY_POLL_A[] = "Poll-A";
static const char CONFIG_TECHNOLOGY_POLL_B[] = "Poll-B";
static const char CONFIG_TECHNOLOGY_POLL_F[] = "Poll-F";
static const char CONFIG_TECHNOLOGY_POLL_V[] = "Poll-V";
static const char CONFIG_TECHNOLOGY_LISTEN_A[] = "Listen-A";
static const char CONFIG_TECHNOLOGY_LISTEN_B[] = "Listen-B";
static const char CONFIG_TECHNOLOGY_LISTEN_F[] = "Listen-F";
static const char CONFIG_TECHNOLOGY_LISTEN_V[] = "Listen-V";

typedef struct nci_technolofy_option {
    const char* name;
    NCI_OPTIONS opt;
} NciTechnologyOption;

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

    /*
     * Protect against nci_sm_switch_to() being invoked by
     * nci_state_enter() or nci_state_reenter()
     */
    self->entering_state++;

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

    /* Allow direct nci_sm_switch_to() calls */
    self->entering_state--;
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

static
void
nci_sm_parse_config(
    NciSm* sm,
    GKeyFile* config)
{
    char* sval = g_key_file_get_string(config, CONFIG_SECTION,
        CONFIG_ENTRY_TECHNOLOGIES, NULL);

    if (sval) {
        static const NciTechnologyOption nci_tech_options[] = {
            /* A and B are always enabled */
            { CONFIG_TECHNOLOGY_TYPE_A, NCI_OPTION_NONE },
            { CONFIG_TECHNOLOGY_TYPE_B, NCI_OPTION_NONE },
            { CONFIG_TECHNOLOGY_TYPE_F, NCI_OPTION_TYPE_F },
            { CONFIG_TECHNOLOGY_TYPE_V, NCI_OPTION_TYPE_V },
            { CONFIG_TECHNOLOGY_POLL_A, NCI_OPTION_NONE },
            { CONFIG_TECHNOLOGY_POLL_B, NCI_OPTION_NONE },
            { CONFIG_TECHNOLOGY_POLL_F, NCI_OPTION_POLL_F },
            { CONFIG_TECHNOLOGY_POLL_V, NCI_OPTION_POLL_V },
            { CONFIG_TECHNOLOGY_LISTEN_A, NCI_OPTION_NONE },
            { CONFIG_TECHNOLOGY_LISTEN_B, NCI_OPTION_NONE },
            { CONFIG_TECHNOLOGY_LISTEN_F, NCI_OPTION_LISTEN_F },
            { CONFIG_TECHNOLOGY_LISTEN_V, NCI_OPTION_LISTEN_V }
        };

        char** techs = g_strsplit_set(sval, CONFIG_LIST_SEPARATORS, -1);
        char** val = techs;

        sm->options = NCI_OPTION_NONE;
        for (val = techs; *val; val++) {
            const NciTechnologyOption* opt = NULL;
            guint i;

            g_strstrip(*val);
            for (i = 0; i < G_N_ELEMENTS(nci_tech_options); i++) {
                if (!g_ascii_strcasecmp(*val, nci_tech_options[i].name)) {
                    opt = nci_tech_options + i;
                    break;
                }
            }

            if (opt) {
                GDEBUG("  %s", opt->name);
                sm->options |= opt->opt;
            } else {
                GWARN("Unexpected technology '%s' in configuration", *val);
            }
        }
        g_strfreev(techs);
        g_free(sval);
    }
}

static
void
nci_sm_load_config(
    NciSm* sm)
{
    if (nci_sm_config_file &&
        g_file_test(nci_sm_config_file, G_FILE_TEST_EXISTS)) {
        GError* error = NULL;
        GKeyFile* config = g_key_file_new();

        if (g_key_file_load_from_file(config, nci_sm_config_file,
            G_KEY_FILE_NONE, &error)) {
            GDEBUG("Parsing %s", nci_sm_config_file);
            nci_sm_parse_config(sm, config);
        } else {
            GERR("Error loading %s: %s", nci_sm_config_file, error->message);
            g_error_free(error);
        }
        g_key_file_unref(config);
    }
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

static
void
nci_sm_switch_internal(
    NciSmObject* self,
    NciState* next)
{
    NciSm* sm = &self->sm;

    if (sm->next_state != next) {
        if (self->next_transition) {
            nci_transition_unref(self->next_transition);
            self->next_transition = NULL;
        }
        if (self->active_transition) {
            NciState* dest = self->active_transition->dest;

            if (dest != next) {
                self->next_transition =
                    nci_state_get_transition(dest, next->state);
                if (self->next_transition) {
                    nci_transition_ref(self->next_transition);
                    nci_sm_set_next_state(self, next);
                } else if (NCI_IS_INTERNAL_STATE(next->state)) {
                    /* Internal states are entered without transition
                     * and take no parameters */
                    nci_sm_enter_state_internal(self, next, NULL);
                } else {
                    GERR("No transition %s -> %s", dest->name, next->name);
                    nci_sm_stall_internal(self, NCI_STALL_ERROR);
                }
            }
        } else {
            NciTransition* direct_transition =
                nci_state_get_transition(sm->last_state, next->state);

            if (direct_transition) {
                /* Found direct transition */
                if (nci_sm_start_transition(self, direct_transition)) {
                    nci_sm_set_next_state(self, next);
                } else {
                    nci_sm_stall_internal(self, NCI_STALL_ERROR);
                }
            } else if (NCI_IS_INTERNAL_STATE(next->state)) {
                /* Internal states are entered without transition and
                 * take no parameters. */
                nci_sm_enter_state_internal(self, next, NULL);
            } else {
                /* Switch to idle state first */
                NciTransition* transition_to_idle =
                    nci_state_get_transition(sm->last_state, NCI_RFST_IDLE);

                if (!transition_to_idle) {
                    /* No direct transition to IDLE, must reset */
                    transition_to_idle = self->reset_transition;
                }
                if (nci_sm_start_transition(self, transition_to_idle)) {
                    NciState* idle = nci_sm_state_by_id(self, NCI_RFST_IDLE);

                    if (next->state == NCI_RFST_IDLE) {
                        nci_sm_set_next_state(self, idle);
                    } else {
                        self->next_transition =
                            nci_state_get_transition(idle, next->state);
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

static
void
nci_sm_switch_destroy(
    gpointer user_data)
{
    NciSmSwitch* data = user_data;

    nci_state_unref(data->state);
    g_slice_free1(sizeof(*data), data);
}

static
gboolean
nci_sm_switch_proc(
    gpointer user_data)
{
    NciSmSwitch* data = user_data;
    NciSmObject* self = data->obj;

    self->pending_switch_id = 0;
    nci_sm_switch_internal(self, data->state);
    return G_SOURCE_REMOVE;
}

void
nci_sm_switch_to(
    NciSm* sm,
    NCI_STATE id)
{
    NciSmObject* self = nci_sm_object(sm);

    if (G_LIKELY(self)) {
        NciState* state = nci_sm_state_by_id(self, id);

        if (G_LIKELY(state)) {
            if (self->pending_switch_id) {
                /* Cancel previously scheduled switch */
                g_source_remove(self->pending_switch_id);
                self->pending_switch_id = 0;
            }
            if (self->entering_state) {
                /* Will do it later on a fresh stack */
                NciSmSwitch* data = g_slice_new(NciSmSwitch);

                data->obj = self;
                data->state = nci_state_ref(state);
                self->pending_switch_id =
                    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                        nci_sm_switch_proc, data, nci_sm_switch_destroy);
            } else {
                nci_sm_switch_internal(self, state);
            }
        }
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
    NciTransition* deactivate_to_discovery;

    sm->io = io;

    /* Default setup */
    nci_sm_add_new_state(sm, nci_state_idle_new);
    nci_sm_add_new_state(sm, nci_state_discovery_new);
    nci_sm_add_new_state(sm, nci_state_listen_active_new);
    nci_sm_add_new_state(sm, nci_state_listen_sleep_new);
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

    /* Reusable deactivate_to_idle */
    deactivate_to_idle = nci_transition_active_to_idle_new(sm);
    nci_sm_add_transition(sm, NCI_RFST_LISTEN_ACTIVE, deactivate_to_idle);
    nci_sm_add_transition(sm, NCI_RFST_POLL_ACTIVE, deactivate_to_idle);
    nci_transition_unref(deactivate_to_idle);

    deactivate_to_idle = nci_transition_deactivate_to_idle_new(sm);
    nci_sm_add_transition(sm, NCI_RFST_DISCOVERY, deactivate_to_idle);
    nci_sm_add_transition(sm, NCI_RFST_W4_ALL_DISCOVERIES, deactivate_to_idle);
    nci_sm_add_transition(sm, NCI_RFST_W4_HOST_SELECT, deactivate_to_idle);
    nci_sm_add_transition(sm, NCI_RFST_LISTEN_SLEEP, deactivate_to_idle);
    nci_transition_unref(deactivate_to_idle);

    /* Reusable deactivate_to_discovery */
    deactivate_to_discovery = nci_transition_deactivate_to_discovery_new(sm);
    nci_sm_add_transition(sm, NCI_RFST_POLL_ACTIVE, deactivate_to_discovery);
    nci_sm_add_transition(sm, NCI_RFST_LISTEN_ACTIVE, deactivate_to_discovery);
    nci_transition_unref(deactivate_to_discovery);

    /* And these are not reusable */
    nci_sm_add_new_transition(sm, NCI_RFST_IDLE,
        nci_transition_idle_to_discovery_new);

    nci_sm_load_config(sm);
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
nci_sm_set_op_mode(
    NciSm* sm,
    NCI_OP_MODE op_mode)
{
    if (G_LIKELY(sm) && sm->op_mode != op_mode) {
        /*
         * If we are really changing the mode, we need to reconfigure NFCC
         * with RF_DISCOVER_MAP_CMD and RF_SET_LISTEN_MODE_ROUTING_CMD, which
         * must be done in RFST_IDLE state. Therefore, we need to switch to
         * RFST_IDLE if we are not there yet.
         */
        sm->op_mode = op_mode;
        nci_sm_switch_to(sm, NCI_RFST_IDLE);
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
        return sm && (sm->op_mode & NFC_OP_MODE_RW);
    case NCI_PROTOCOL_ISO_DEP:
        return sm && (sm->op_mode & (NFC_OP_MODE_RW | NFC_OP_MODE_CE));
    case NCI_PROTOCOL_NFC_DEP:
        return sm && (sm->op_mode & NFC_OP_MODE_PEER);
    case NCI_PROTOCOL_UNDETERMINED:
    case NCI_PROTOCOL_T1T:
    case NCI_PROTOCOL_T3T:
    case NCI_PROTOCOL_PROPRIETARY:
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
        NciSar* sar = nci_sm_sar(sm);

        nci_sar_set_max_data_payload_size(sar, ntf->max_data_packet_size);
        nci_sar_set_initial_credits(sar, NCI_STATIC_RF_CONN_ID,
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
    GWARN("Failed to parse RF_DEACTIVATE_NTF");
    nci_sm_stall(sm, NCI_STALL_ERROR);
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

    /* Only poll modes by default */
    sm->op_mode = NFC_OP_MODE_RW | NFC_OP_MODE_PEER | NFC_OP_MODE_POLL;
    sm->options = NCI_OPTIONS_DEFAULT;
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

    if (self->pending_switch_id) {
        g_source_remove(self->pending_switch_id);
        self->pending_switch_id = 0;
    }
    nci_sm_finish_active_transition(self);
    nci_state_leave(self->active_state);
    if (sm->rf_interfaces) {
        g_bytes_unref(sm->rf_interfaces);
    }
    nci_transition_unref(self->next_transition);
    g_ptr_array_free(self->transitions, TRUE);
    g_ptr_array_free(self->states, TRUE);
    nci_transition_unref(self->reset_transition);
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
