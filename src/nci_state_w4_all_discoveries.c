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
#include "nci_state_impl.h"
#include "nci_param_w4_all_discoveries.h"
#include "nci_param_w4_host_select.h"
#include "nci_util.h"

typedef NciStateClass NciStateW4AllDiscoveriesClass;
typedef struct nci_state_w4_all_discoveries {
    NciState state;
    GPtrArray* discoveries;
} NciStateW4AllDiscoveries;

G_DEFINE_TYPE(NciStateW4AllDiscoveries, nci_state_w4_all_discoveries,
    NCI_TYPE_STATE)
#define THIS_TYPE (nci_state_w4_all_discoveries_get_type())
#define PARENT_CLASS (nci_state_w4_all_discoveries_parent_class)
#define NCI_STATE_W4_ALL_DISCOVERIES(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        THIS_TYPE, NciStateW4AllDiscoveries))

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
void
nci_state_w4_all_discoveries_handle_discovery(
    NciStateW4AllDiscoveries* self,
    const NciDiscoveryNtf* ntf)
{
    /* Store discovery info in self->discoveries */
    g_ptr_array_add(self->discoveries, nci_discovery_ntf_copy(ntf));

    /*
     * 5.2.3 State RFST_W4_ALL_DISCOVERIES
     *
     * ...
     * Discover notifications with Notification type set to 2 SHALL NOT
     * change the state. When the NFCC sends the last RF_DISCOVER_NTF
     * (Notification Type not equal to 2) to the DH, the state is
     * changed to RFST_W4_HOST_SELECT.
     */
    if (ntf->last) {
        NciSm* sm = nci_state_sm(&self->state);
        NciParam* param = NCI_PARAM(nci_param_w4_host_select_new(
            (const NciDiscoveryNtf**)self->discoveries->pdata,
            self->discoveries->len));

        nci_sm_enter_state(sm, NCI_RFST_W4_HOST_SELECT, param);
        nci_param_unref(param);
    }
}

static
void
nci_state_w4_all_discoveries_start(
    NciStateW4AllDiscoveries* self,
    NciParam* param)
{
    g_ptr_array_set_size(self->discoveries, 0);
    if (NCI_IS_PARAM_W4_ALL_DISCOVERIES(param)) {
        nci_param_ref(param);
        nci_state_w4_all_discoveries_handle_discovery(self,
            NCI_PARAM_W4_ALL_DISCOVERIES(param)->ntf);
        nci_param_unref(param);
    }
}

static
void
nci_state_w4_all_discoveries_discover_ntf(
    NciStateW4AllDiscoveries* self,
    const GUtilData* payload)
{
    NciDiscoveryNtf ntf;
    NciModeParam param;

    if (nci_parse_discover_ntf(&ntf, &param, payload->bytes, payload->size)) {
        nci_state_w4_all_discoveries_handle_discovery(self, &ntf);
    }
}

/*==========================================================================*
 * Interface
 *==========================================================================*/

NciState*
nci_state_w4_all_discoveries_new(
    NciSm* sm)
{
    NciState* self = g_object_new(THIS_TYPE, NULL);

    nci_state_init_base(self, sm, NCI_RFST_W4_ALL_DISCOVERIES,
        "RFST_W4_ALL_DISCOVERIES");
    return self;
}

/*==========================================================================*
 * Methods
 *==========================================================================*/

static
void
nci_state_w4_all_discoveries_enter(
    NciState* state,
    NciParam* param)
{
    NciStateW4AllDiscoveries* self = NCI_STATE_W4_ALL_DISCOVERIES(state);

    nci_state_w4_all_discoveries_start(self, param);
    NCI_STATE_CLASS(PARENT_CLASS)->enter(state, param);
}

static
void
nci_state_w4_all_discoveries_reenter(
    NciState* state,
    NciParam* param)
{
    NciStateW4AllDiscoveries* self = NCI_STATE_W4_ALL_DISCOVERIES(state);

    nci_state_w4_all_discoveries_start(self, param);
    NCI_STATE_CLASS(PARENT_CLASS)->reenter(state, param);
}

static
void
nci_state_w4_all_discoveries_leave(
    NciState* state)
{
    NciStateW4AllDiscoveries* self = NCI_STATE_W4_ALL_DISCOVERIES(state);

    /* Release the memory */
    g_ptr_array_set_size(self->discoveries, 0);
    NCI_STATE_CLASS(PARENT_CLASS)->leave(state);
}

static
void
nci_state_w4_all_discoveries_handle_ntf(
    NciState* state,
    guint8 gid,
    guint8 oid,
    const GUtilData* payload)
{
    switch (gid) {
    case NCI_GID_RF:
        switch (oid) {
        case NCI_OID_RF_DISCOVER:
            nci_state_w4_all_discoveries_discover_ntf
                (NCI_STATE_W4_ALL_DISCOVERIES(state), payload);
            return;
        }
        break;
    }
    NCI_STATE_CLASS(PARENT_CLASS)->handle_ntf(state, gid, oid, payload);
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
void
nci_state_w4_all_discoveries_init(
    NciStateW4AllDiscoveries* self)
{
    self->discoveries = g_ptr_array_new_with_free_func(g_free);
}

static
void
nci_state_w4_all_discoveries_finalize(
    GObject* object)
{
    NciStateW4AllDiscoveries* self = NCI_STATE_W4_ALL_DISCOVERIES(object);

    g_ptr_array_free(self->discoveries, TRUE);
    G_OBJECT_CLASS(PARENT_CLASS)->finalize(object);
}

static
void
nci_state_w4_all_discoveries_class_init(
    NciStateW4AllDiscoveriesClass* klass)
{
    klass->enter = nci_state_w4_all_discoveries_enter;
    klass->reenter = nci_state_w4_all_discoveries_reenter;
    klass->leave = nci_state_w4_all_discoveries_leave;
    klass->handle_ntf = nci_state_w4_all_discoveries_handle_ntf;
    G_OBJECT_CLASS(klass)->finalize = nci_state_w4_all_discoveries_finalize;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
