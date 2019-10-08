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
 * 5.2.2 State RFST_DISCOVERY
 * 5.2.3 State RFST_W4_ALL_DISCOVERIES
 * 5.2.4 State RFST_W4_HOST_SELECT
 *
 * ...
 *
 * If the DH sends RF_DEACTIVATE_CMD, the NFCC SHALL ignore the Deactivation
 * Type parameter, stop the RF Discovery Process, and send RF_DEACTIVATE_RSP.
 * The state will then change to RFST_IDLE.
 *
 *==========================================================================*/

typedef NciTransition NciTransitionDeactivateToIdle;
typedef NciTransitionClass NciTransitionDeactivateToIdleClass;

G_DEFINE_TYPE(NciTransitionDeactivateToIdle, nci_transition_deactivate_to_idle,
    NCI_TYPE_TRANSITION)
#define THIS_TYPE (nci_transition_deactivate_to_idle_get_type())
#define PARENT_CLASS (nci_transition_deactivate_to_idle_parent_class)

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
void
nci_transition_deactivate_to_idle_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* self)
{
    if (status == NCI_REQUEST_CANCELLED || !nci_transition_active(self)) {
        GDEBUG("RF_DEACTIVATE cancelled");
    } else if (status == NCI_REQUEST_TIMEOUT) {
        GDEBUG("RF_DEACTIVATE timed out");
        nci_transition_stall(self, NCI_STALL_ERROR);
    } else {
        const guint8* pkt = payload->bytes;
        const guint len = payload->size;

        /*
         * Table 62: Control Messages for RF Interface Deactivation
         *
         * RF_DEACTIVATE_RSP
         *
         * +=========================================================+
         * | Offset | Size | Description                             |
         * +=========================================================+
         * | 0      | 1    | Status                                  |
         * +=========================================================+
         */
        if (len > 0 && pkt[0] == NCI_STATUS_OK) {
            GDEBUG("%c RF_DEACTIVATE_RSP ok", DIR_IN);
            nci_transition_finish(self, NULL);
        } else {
            if (len > 0) {
                GWARN("%c RF_DEACTIVATE_RSP error %u", DIR_IN, pkt[0]);
            } else {
                GWARN("%c Broken RF_DEACTIVATE_RSP", DIR_IN);
            }
            nci_transition_stall(self, NCI_STALL_ERROR);
        }
    }
}

/*==========================================================================*
 * Interface
 *==========================================================================*/

NciTransition* 
nci_transition_deactivate_to_idle_new(
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
gboolean
nci_transition_deactivate_to_idle_start(
    NciTransition* self)
{
    return nci_transition_deactivate_to_idle(self,
        nci_transition_deactivate_to_idle_rsp);
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
void
nci_transition_deactivate_to_idle_init(
    NciTransitionDeactivateToIdle* self)
{
}

static
void
nci_transition_deactivate_to_idle_class_init(
    NciTransitionDeactivateToIdleClass* klass)
{
    klass->start = nci_transition_deactivate_to_idle_start;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
