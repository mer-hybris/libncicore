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

#include "nci_transition.h"
#include "nci_transition_impl.h"
#include "nci_state.h"
#include "nci_sm.h"
#include "nci_log.h"

struct nci_transition_priv {
    NciSm* sm; /* Weak reference */
};

G_DEFINE_ABSTRACT_TYPE(NciTransition, nci_transition, G_TYPE_OBJECT)
#define NCI_TRANSITION_GET_CLASS(obj) G_TYPE_INSTANCE_GET_CLASS((obj), \
        NCI_TYPE_TRANSITION, NciTransitionClass)

/*==========================================================================*
 * Interface
 *==========================================================================*/

NciTransition*
nci_transition_ref(
    NciTransition* self)
{
    if (G_LIKELY(self)) {
        g_object_ref(NCI_TRANSITION(self));
    }
    return self;
}

void
nci_transition_unref(
    NciTransition* self)
{
    if (G_LIKELY(self)) {
        g_object_unref(NCI_TRANSITION(self));
    }
}

void
nci_transition_init_base(
    NciTransition* self,
    NciSm* sm,
    NciState* dest)
{
    NciTransitionPriv* priv = self->priv;

    self->dest = nci_state_ref(dest);
    priv->sm = sm;
    nci_sm_add_weak_pointer(&priv->sm);
}

NciSm*
nci_transition_sm(
    NciTransition* self)
{
    return G_LIKELY(self) ? self->priv->sm : NULL;
}

void
nci_transition_stall(
    NciTransition* self,
    NCI_STALL stall)
{
    nci_sm_stall(nci_transition_sm(self), stall);
}

gboolean
nci_transition_active(
    NciTransition* self)
{
    return nci_sm_active_transition(nci_transition_sm(self), self);
}

void
nci_transition_finish(
    NciTransition* self,
    void* param)
{
    if (G_LIKELY(self)) {
        nci_sm_enter_state(self->priv->sm, self->dest->state, param);
    }
}

gboolean
nci_transition_start(
    NciTransition* self)
{
    if (G_LIKELY(self)) {
        if (NCI_TRANSITION_GET_CLASS(self)->start(self)) {
            return TRUE;
        }
    }
    return FALSE;
}

void
nci_transition_finished(
    NciTransition* self)
{
    if (G_LIKELY(self)) {
        NCI_TRANSITION_GET_CLASS(self)->finished(self);
    }
}

void
nci_transition_handle_ntf(
    NciTransition* self,
    guint8 gid,
    guint8 oid,
    const GUtilData* payload)
{
    if (G_LIKELY(self)) {
        NCI_TRANSITION_GET_CLASS(self)->handle_ntf(self, gid, oid, payload);
    }
}

gboolean
nci_transition_send_command(
    NciTransition* self,
    guint8 gid,
    guint8 oid,
    GBytes* payload,
    NciTransitionResponseFunc resp)
{
    return G_LIKELY(self) && nci_sm_send_command(self->priv->sm,
        gid, oid, payload, (NciSmResponseFunc)resp, self);
}

gboolean
nci_transition_send_command_static(
    NciTransition* self,
    guint8 gid,
    guint8 oid,
    const void* payload,
    gsize payload_len,
    NciTransitionResponseFunc resp)
{
    return G_LIKELY(self) && nci_sm_send_command_static(self->priv->sm,
        gid, oid, payload, payload_len, (NciSmResponseFunc)resp, self);
}

/*
 * Table 62: Control Messages for RF Interface Deactivation
 *
 * RF_DEACTIVATE_CMD
 *
 * +=========================================================+
 * | Offset | Size | Description                             |
 * +=========================================================+
 * | 0      | 1    | Deactivation Type                       |
 * +=========================================================+
 */
gboolean
nci_transition_deactivate_to_idle(
    NciTransition* self,
    NciTransitionResponseFunc resp)
{
    static const guint8 cmd[] = { NCI_DEACTIVATE_TYPE_IDLE };

    GDEBUG("%c RF_DEACTIVATE_CMD (Idle)", DIR_OUT);
    return nci_transition_send_command_static(self, NCI_GID_RF,
        NCI_OID_RF_DEACTIVATE, cmd, sizeof(cmd), resp);
}

gboolean
nci_transition_deactivate_to_discovery(
    NciTransition* self,
    NciTransitionResponseFunc resp)
{
    static const guint8 cmd[] = { NCI_DEACTIVATE_TYPE_DISCOVERY };

    GDEBUG("%c RF_DEACTIVATE_CMD (Discovery)", DIR_OUT);
    return nci_transition_send_command_static(self, NCI_GID_RF,
        NCI_OID_RF_DEACTIVATE, cmd, sizeof(cmd), resp);
}

/*==========================================================================*
 * Methods
 *==========================================================================*/

static
gboolean
nci_transition_default_start(
    NciTransition* self)
{
    GERR("Non-startable transition!");
    return FALSE;
}

static
void
nci_transition_default_finished(
    NciTransition* self)
{
}

static
void
nci_transition_default_handle_ntf(
    NciTransition* self,
    guint8 gid,
    guint8 oid,
    const GUtilData* payload)
{
    GDEBUG("Notification 0x%02x/0x%02x is ignored in transition", gid, oid);
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
void
nci_transition_init(
    NciTransition* self)
{
    NciTransitionPriv* priv =  G_TYPE_INSTANCE_GET_PRIVATE(self,
        NCI_TYPE_TRANSITION, NciTransitionPriv);

    self->priv = priv;
}

static
void
nci_transition_finalize(
    GObject* object)
{
    NciTransition* self = NCI_TRANSITION(object);
    NciTransitionPriv* priv = self->priv;

    nci_state_unref(self->dest);
    nci_sm_remove_weak_pointer(&priv->sm);
    G_OBJECT_CLASS(nci_transition_parent_class)->finalize(object);
}

static
void
nci_transition_class_init(
    NciTransitionClass* klass)
{
    g_type_class_add_private(klass, sizeof(NciTransitionPriv));
    klass->start = nci_transition_default_start;
    klass->finished = nci_transition_default_finished;
    klass->handle_ntf = nci_transition_default_handle_ntf;
    G_OBJECT_CLASS(klass)->finalize = nci_transition_finalize;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
