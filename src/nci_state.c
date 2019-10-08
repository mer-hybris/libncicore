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

#include "nci_state_p.h"
#include "nci_state_impl.h"
#include "nci_sm.h"
#include "nci_transition.h"
#include "nci_log.h"

struct nci_state_priv {
    NciSm* sm; /* Weak reference */
    GHashTable* transitions;
};

G_DEFINE_TYPE(NciState, nci_state, G_TYPE_OBJECT)
#define NCI_STATE_GET_CLASS(obj) G_TYPE_INSTANCE_GET_CLASS((obj), \
        NCI_TYPE_STATE, NciStateClass)

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
void
nci_state_transition_gone(
    gpointer state,
    GObject* dead_transition)
{
    NciState* self = NCI_STATE(state);
    NciStatePriv* priv = self->priv;
    GHashTableIter it;
    gpointer value;

    g_hash_table_iter_init(&it, priv->transitions);
    while (g_hash_table_iter_next(&it, NULL, &value)) {
        if (dead_transition == value) {
            g_hash_table_iter_remove(&it);
            return;
        }
    }
    GVERBOSE("Unexpected dead transition %p", dead_transition);
}

static
void
nci_state_generic_error_ntf(
    NciState* self,
    const GUtilData* payload)
{
    if (payload->size == 1) {
        GWARN("Generic error 0x%02x", payload->bytes[0]);
    } else {
        GWARN("Failed to parse CORE_GENERIC_ERROR_NTF");
    }
}

/*==========================================================================*
 * Interface
 *==========================================================================*/

NciState*
nci_state_ref(
    NciState* self)
{
    if (G_LIKELY(self)) {
        g_object_ref(NCI_STATE(self));
    }
    return self;
}

void
nci_state_unref(
    NciState* self)
{
    if (G_LIKELY(self)) {
        g_object_unref(NCI_STATE(self));
    }
}

/*==========================================================================*
 * Internal states
 *==========================================================================*/

static
NciState*
nci_state_new(
    NciSm* sm,
    NCI_STATE state,
    const char* name)
{
    NciState* self = g_object_new(NCI_TYPE_STATE, NULL);

    nci_state_init_base(self, sm, state, name);
    return self;
}

NciState*
nci_state_init_new(
    NciSm* sm)
{
    return nci_state_new(sm, NCI_STATE_INIT, "INIT");
}

NciState*
nci_state_error_new(
    NciSm* sm)
{
    return nci_state_new(sm, NCI_STATE_ERROR, "ERROR");
}

NciState*
nci_state_stop_new(
    NciSm* sm)
{
    return nci_state_new(sm, NCI_STATE_STOP, "STOP");
}

/*==========================================================================*
 * Trivial states
 *==========================================================================*/

NciState*
nci_state_idle_new(
    NciSm* sm)
{
    return nci_state_new(sm, NCI_RFST_IDLE, "RFST_IDLE");
}

/*==========================================================================*
 * Internal interface
 *==========================================================================*/

void
nci_state_init_base(
    NciState* self,
    NciSm* sm,
    NCI_STATE state,
    const char* name)
{
    NciStatePriv* priv = self->priv;

    /* Assume that name points to static buffer */
    self->name = name;
    self->state = state;
    priv->sm = sm;
    nci_sm_add_weak_pointer(&priv->sm);
}

gboolean
nci_state_send_command(
    NciState* self,
    guint8 gid,
    guint8 oid,
    GBytes* payload,
    NciSmResponseFunc resp,
    gpointer user_data)
{
    return G_LIKELY(self) && nci_sm_send_command(self->priv->sm,
        gid, oid, payload, resp, user_data);
}

NciSm*
nci_state_sm(
    NciState* self)
{
    return G_LIKELY(self) ? self->priv->sm : NULL; 
}

void
nci_state_add_transition(
    NciState* self,
    NciTransition* transition)
{
    if (G_LIKELY(self) && G_LIKELY(transition)) {
        NciStatePriv* priv = self->priv;

        g_hash_table_insert(priv->transitions,
            GUINT_TO_POINTER(transition->dest->state), transition);
        g_object_weak_ref(G_OBJECT(transition),
            nci_state_transition_gone, self);
    }
}

NciTransition*
nci_state_get_transition(
    NciState* self,
    NCI_STATE dest)
{
    if (G_LIKELY(self)) {
        NciStatePriv* priv = self->priv;

        return g_hash_table_lookup(priv->transitions, GUINT_TO_POINTER(dest));
    }
    return NULL;
}

void
nci_state_enter(
    NciState* self,
    void* param)
{
    if (G_LIKELY(self)) {
        NCI_STATE_GET_CLASS(self)->enter(self, param);
    }
}

void
nci_state_reenter(
    NciState* self,
    void* param)
{
    if (G_LIKELY(self)) {
        NCI_STATE_GET_CLASS(self)->reenter(self, param);
    }
}

void
nci_state_leave(
    NciState* self)
{
    if (G_LIKELY(self)) {
        NCI_STATE_GET_CLASS(self)->leave(self);
    }
}

void
nci_state_handle_ntf(
    NciState* self,
    guint8 gid,
    guint8 oid,
    const GUtilData* payload)
{
    if (G_LIKELY(self)) {
        NCI_STATE_GET_CLASS(self)->handle_ntf(self, gid, oid, payload);
    }
}

/*==========================================================================*
 * Methods
 *==========================================================================*/

static
void
nci_state_default_enter(
    NciState* self,
    NciParam* param)
{
    GVERBOSE("Entered %s state", self->name);
    self->active = TRUE;
}

static
void
nci_state_default_reenter(
    NciState* self,
    NciParam* param)
{
    GVERBOSE("Re-entered %s state", self->name);
    GASSERT(self->active);
}

static
void
nci_state_default_leave(
    NciState* self)
{
    GVERBOSE("Left %s state", self->name);
    self->active = FALSE;
}

static
void
nci_state_default_handle_ntf(
    NciState* self,
    guint8 gid,
    guint8 oid,
    const GUtilData* payload)
{
    switch (gid) {
    case NCI_GID_CORE:
        switch (oid) {
        case NCI_OID_CORE_CONN_CREDITS:
            nci_sm_handle_conn_credits_ntf(nci_state_sm(self), payload);
            return;
        case NCI_OID_CORE_GENERIC_ERROR:
            nci_state_generic_error_ntf(self, payload);
            return;
        }
        break;
    }
    GDEBUG("Notification 0x%02x/0x%02x is ignored in %s state", gid, oid,
        self->name);
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
void
nci_state_init(
    NciState* self)
{
    NciStatePriv* priv =  G_TYPE_INSTANCE_GET_PRIVATE(self,
        NCI_TYPE_STATE, NciStatePriv);

    self->priv = priv;
    priv->transitions = g_hash_table_new(g_direct_hash, g_direct_equal);
}

static
void
nci_state_finalize(
    GObject* object)
{
    NciState* self = NCI_STATE(object);
    NciStatePriv* priv = self->priv;
    GHashTableIter it;
    gpointer value;

    nci_sm_remove_weak_pointer(&priv->sm);
    g_hash_table_iter_init(&it, priv->transitions);
    while (g_hash_table_iter_next(&it, NULL, &value)) {
        g_object_weak_unref(value, nci_state_transition_gone, self);
        g_hash_table_iter_remove(&it);
    }
    g_hash_table_destroy(priv->transitions);
    G_OBJECT_CLASS(nci_state_parent_class)->finalize(object);
}

static
void
nci_state_class_init(
    NciStateClass* klass)
{
    g_type_class_add_private(klass, sizeof(NciStatePriv));
    klass->enter = nci_state_default_enter;
    klass->reenter = nci_state_default_reenter;
    klass->leave = nci_state_default_leave;
    klass->handle_ntf = nci_state_default_handle_ntf;
    G_OBJECT_CLASS(klass)->finalize = nci_state_finalize;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
