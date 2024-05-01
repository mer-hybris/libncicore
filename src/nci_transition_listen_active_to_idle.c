/*
 * Copyright (C) 2019-2024 Slava Monich <slava@monich.com>
 * Copyright (C) 2019-2020 Jolla Ltd.
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer
 *     in the documentation and/or other materials provided with the
 *     distribution.
 *
 *  3. Neither the names of the copyright holders nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * any official policies, either expressed or implied.
 */

#include "nci_transition_impl.h"
#include "nci_util_p.h"
#include "nci_sm.h"
#include "nci_log.h"

#include <gutil_macros.h>

/*==========================================================================*
 *
 * [NFCForum-TS-NCI-1.0]
 * 5.2.6 State RFST_LISTEN_ACTIVE
 *
 * ...
 * If the DH sends RF_DEACTIVATE_CMD (Idle Mode), the NFCC SHALL send
 * RF_DEACTIVATE_RSP followed by RF_DEACTIVATE_NTF (Idle Mode, DH Request)
 * upon successful deactivation. The state will then change to RFST_IDLE.
 *==========================================================================*/

/*
 * Here comes a slightly tricky part.
 *
 * According to the NCI spec the call/response/event sequence is supposed
 * to go like this:
 *
 * < RF_DEACTIVATE_CMD (Idle)
 * > RF_DEACTIVATE_RSP ok
 * > RF_DEACTIVATE_NTF (Idle, DH_Request)
 *
 * and that's what happens most of the time. However, in those rare cases
 * when e.g. link loss occurs in LISTEN_ACTIVE state at more or less the
 * same time when RF_DEACTIVATE_CMD is submitted, something like this has
 * been seen in real life:
 *
 * < RF_DEACTIVATE_CMD (Idle)
 * > RF_DEACTIVATE_NTF (Sleep, Endpoint_Request)
 * > RF_DEACTIVATE_NTF (Discovery, RF_Link_Loss)
 * > RF_DEACTIVATE_RSP ok
 *
 * and then RF_DEACTIVATE_NTF (Idle, DH_Request) never arrives.
 *
 * Apparently, in this case NFCC changes its state to LISTEN_SLEEP and
 * then DISCOVERY state before it receives RF_DEACTIVATE_CMD, and handles
 * RF_DEACTIVATE_CMD according to the rules of that new (e.g. DISCOVERY)
 * state. And transition from the new state to IDLE does't involve
 * RF_DEACTIVATE_NTF, which explains  why it never arrives.
 */

typedef NciTransitionClass NciTransitionListenActiveToIdleClass;
typedef struct nci_transition_listen_active_to_idle {
    NciTransition transition;
    NciRfDeactivateNtf* ntf;
    gboolean expecting_ntf;
} NciTransitionListenActiveToIdle;

#define THIS_TYPE nci_transition_listen_active_to_idle_get_type()
#define PARENT_CLASS nci_transition_listen_active_to_idle_parent_class
#define PARENT_CLASS_CALL(method) (NCI_TRANSITION_CLASS(PARENT_CLASS)->method)
#define THIS(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, THIS_TYPE, \
    NciTransitionListenActiveToIdle)

GType THIS_TYPE NCI_INTERNAL;
G_DEFINE_TYPE(NciTransitionListenActiveToIdle,
    nci_transition_listen_active_to_idle, NCI_TYPE_TRANSITION)

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
void
nci_transition_listen_active_to_idle_reset(
    NciTransitionListenActiveToIdle* self)
{
    self->expecting_ntf = FALSE;
    if (self->ntf) {
        gutil_slice_free(self->ntf);
        self->ntf = NULL;
    }
}

static
void
nci_transition_listen_active_to_idle_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* transition)
{
    if (status == NCI_REQUEST_CANCELLED || !nci_transition_active(transition)) {
        GDEBUG("RF_DEACTIVATE (Idle) cancelled");
    } else if (status == NCI_REQUEST_TIMEOUT) {
        GDEBUG("RF_DEACTIVATE (Idle) timed out");
        nci_transition_error(transition);
    } else if (status == NCI_REQUEST_SUCCESS) {
        const guint8* rsp = payload->bytes;
        const guint len = payload->size;

        if (len == 1 && rsp[0] == NCI_STATUS_OK) {
            NciTransitionListenActiveToIdle* self = THIS(transition);

            if (self->ntf) {
                /*
                 * It doesn't really matter what was the intermediate state.
                 * Any RF_DEACTIVATE_NTF arriving between RF_DEACTIVATE_CMD
                 * and RF_DEACTIVATE_RSP means that another RF_DEACTIVATE_NTF
                 * won't arrive.
                 */
                GDEBUG("%c RF_DEACTIVATE_RSP (Idle) ok", DIR_IN);
                nci_transition_finish(transition, NULL);
            } else {
                /* else keep waiting for RF_DEACTIVATE_NTF */
                self->expecting_ntf = TRUE;
                GDEBUG("%c RF_DEACTIVATE_RSP (Idle) ok, waiting for NTF",
                    DIR_IN);
            }
        } else {
            if (len > 0) {
                GWARN("%c RF_DEACTIVATE_RSP (Idle) error %u", DIR_IN, rsp[0]);
            } else {
                GWARN("%c Broken RF_DEACTIVATE_RSP (Idle)", DIR_IN);
            }
            nci_transition_error(transition);
        }
    }
}

/*==========================================================================*
 * Interface
 *==========================================================================*/

NciTransition*
nci_transition_listen_active_to_idle_new(
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
nci_transition_listen_active_to_idle_handle_ntf(
    NciTransition* transition,
    guint8 gid,
    guint8 oid,
    const GUtilData* payload)
{
    NciRfDeactivateNtf ntf;

    switch (gid) {
    case NCI_GID_RF:
        switch (oid) {
        case NCI_OID_RF_DEACTIVATE:
            if (nci_parse_rf_deactivate_ntf(&ntf, payload)) {
                NciTransitionListenActiveToIdle* self = THIS(transition);

                if (self->expecting_ntf &&
                    ntf.type == NCI_DEACTIVATE_TYPE_IDLE &&
                    ntf.reason == NCI_DEACTIVATION_REASON_DH_REQUEST) {
                    /* We have already received RF_DEACTIVATE_RSP */
                    nci_transition_finish(transition, NULL);
                } else {
                    /* Store this RF_DEACTIVATE_NTF and wait for RSP */
                    gutil_slice_free(self->ntf);
                    self->ntf = g_slice_dup(NciRfDeactivateNtf, &ntf);
                }
            } else {
                nci_transition_error(transition);
            }
            return;
        }
        break;
    }
    PARENT_CLASS_CALL(handle_ntf)(transition, gid, oid, payload);
}

static
gboolean
nci_transition_listen_active_to_idle_start(
    NciTransition* transition)
{
    nci_transition_listen_active_to_idle_reset(THIS(transition));
    return PARENT_CLASS_CALL(start)(transition) &&
        nci_transition_deactivate_to_idle(transition,
        nci_transition_listen_active_to_idle_rsp);
}

static
void
nci_transition_listen_active_to_idle_finished(
    NciTransition* transition)
{
    nci_transition_listen_active_to_idle_reset(THIS(transition));
    PARENT_CLASS_CALL(finished)(transition);
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
void
nci_transition_listen_active_to_idle_finalize(
    GObject* object)
{
    nci_transition_listen_active_to_idle_reset(THIS(object));
    G_OBJECT_CLASS(PARENT_CLASS)->finalize(object);
}

static
void
nci_transition_listen_active_to_idle_init(
    NciTransitionListenActiveToIdle* self)
{
}

static
void
nci_transition_listen_active_to_idle_class_init(
    NciTransitionListenActiveToIdleClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize =
        nci_transition_listen_active_to_idle_finalize;
    klass->start = nci_transition_listen_active_to_idle_start;
    klass->handle_ntf = nci_transition_listen_active_to_idle_handle_ntf;
    klass->finished = nci_transition_listen_active_to_idle_finished;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
