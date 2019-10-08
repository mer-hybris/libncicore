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
#include "nci_sm.h"
#include "nci_log.h"

/*==========================================================================*
 *
 * 5.2.5 State RFST_POLL_ACTIVE
 *
 * ...
 * If the DH sends RF_DEACTIVATE_CMD (Idle Mode), the NFCC SHALL send
 * RF_DEACTIVATE_RSP followed by RF_DEACTIVATE_NTF (Idle Mode, DH Request)
 * upon successful deactivation. The state will then change to RFST_IDLE.
 *
 *==========================================================================*/

typedef NciTransition NciTransitionPollActiveToIdle;
typedef NciTransitionClass NciTransitionPollActiveToIdleClass;

G_DEFINE_TYPE(NciTransitionPollActiveToIdle,
    nci_transition_poll_active_to_idle, NCI_TYPE_TRANSITION)
#define THIS_TYPE (nci_transition_poll_active_to_idle_get_type())
#define PARENT_CLASS (nci_transition_poll_active_to_idle_parent_class)

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
void
nci_transition_poll_active_to_idle_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* self)
{
    if (status == NCI_REQUEST_CANCELLED || !nci_transition_active(self)) {
        GDEBUG("RF_DEACTIVATE (Idle) cancelled");
    } else if (status == NCI_REQUEST_TIMEOUT) {
        GDEBUG("RF_DEACTIVATE (Idle) timed out");
        nci_transition_stall(self, NCI_STALL_ERROR);
    } else if (status == NCI_REQUEST_SUCCESS) {
        const guint8* rsp = payload->bytes;
        const guint len = payload->size;

        if (len == 1 && rsp[0] == NCI_STATUS_OK) {
            GDEBUG("%c RF_DEACTIVATE_RSP (Idle) ok", DIR_IN);
            /* Wait for RF_DEACTIVATE_NTF */
        } else {
            if (len > 0) {
                GWARN("%c RF_DEACTIVATE_RSP (Idle) error %u", DIR_IN, rsp[0]);
            } else {
                GWARN("%c Broken RF_DEACTIVATE_RSP (Idle)", DIR_IN);
            }
            nci_transition_stall(self, NCI_STALL_ERROR);
        }
    }
}

/*==========================================================================*
 * Interface
 *==========================================================================*/

NciTransition* 
nci_transition_poll_active_to_idle_new(
    NciSm* sm)
{
    NciState* dest = nci_sm_get_state(sm, NCI_RFST_IDLE);

    if (dest) {
        NciTransition* self = g_object_new(THIS_TYPE, NULL);

        nci_transition_init_base(self, sm, dest);
        return self;
    }
    return NULL;
}

/*==========================================================================*
 * Methods
 *==========================================================================*/

static
void
nci_transition_poll_active_to_idle_handle_ntf(
    NciTransition* self,
    guint8 gid,
    guint8 oid,
    const GUtilData* payload)
{
    switch (gid) {
    case NCI_GID_RF:
        switch (oid) {
        case NCI_OID_RF_DEACTIVATE:
            nci_sm_handle_rf_deactivate_ntf(nci_transition_sm(self), payload);
            return;
        }
        break;
    }
    NCI_TRANSITION_CLASS(PARENT_CLASS)->handle_ntf(self, gid, oid, payload);
}

static
gboolean
nci_transition_poll_active_to_idle_start(
    NciTransition* self)
{
    return nci_transition_deactivate_to_idle(self,
        nci_transition_poll_active_to_idle_rsp);
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
void
nci_transition_poll_active_to_idle_init(
    NciTransitionPollActiveToIdle* self)
{
}

static
void
nci_transition_poll_active_to_idle_class_init(
    NciTransitionPollActiveToIdleClass* klass)
{
    klass->start = nci_transition_poll_active_to_idle_start;
    klass->handle_ntf = nci_transition_poll_active_to_idle_handle_ntf;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
