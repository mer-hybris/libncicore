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
#include "nci_log.h"

typedef NciState NciStatePollActive;
typedef NciStateClass NciStatePollActiveClass;

G_DEFINE_TYPE(NciStatePollActive, nci_state_poll_active, NCI_TYPE_STATE)
#define THIS_TYPE (nci_state_poll_active_get_type())
#define PARENT_CLASS (nci_state_poll_active_parent_class)

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
gboolean
nci_state_poll_active_interface_error_ntf(
    NciState* self,
    const GUtilData* payload)
{
    const guint8* pkt = payload->bytes;
    const guint len = payload->size;

    /*
     * Table 19: Control Messages for Interface Error
     *
     * CORE_INTERFACE_ERROR_NTF
     *
     * +=========================================================+
     * | Offset | Size | Description                             |
     * +=========================================================+
     * | 0      | 1    | Status                                  |
     * | 1      | 1    | Conn ID                                 |
     * +=========================================================+
     *
     * 5.2.5 State RFST_POLL_ACTIVE
     *
     * ...
     * When using the ISO-DEP or NFC-DEP RF interface, and the NFCC
     * detects an error during the RF communication, it SHALL notify
     * the DH sending CORE_INTERFACE_ERROR_NTF, using the appropriate
     * status out of RF_TRANSMISSION_ERROR, RF_PROTOCOL_ERROR and
     * RF_TIMEOUT_ERROR. The state will then remain RFST_POLL_ACTIVE.
     */
    if (len == 2) {
        switch (pkt[0]) {
        case NCI_RF_TRANSMISSION_ERROR:
            GDEBUG("CORE_INTERFACE_ERROR_NTF (Transmission Error)");
            return TRUE;
        case NCI_RF_PROTOCOL_ERROR:
            GDEBUG("CORE_INTERFACE_ERROR_NTF (Protocol Error)");
            return TRUE;
        case NCI_RF_TIMEOUT_ERROR:
            GDEBUG("CORE_INTERFACE_ERROR_NTF (Timeout)");
            return TRUE;
        }
    }
    /* Unrecornized notification */
    return FALSE;
}

/*==========================================================================*
 * Interface
 *==========================================================================*/

NciState*
nci_state_poll_active_new(
    NciSm* sm)
{
    NciState* self = g_object_new(THIS_TYPE, NULL);

    nci_state_init_base(self, sm, NCI_RFST_POLL_ACTIVE, "RFST_POLL_ACTIVE");
    return self;
}

/*==========================================================================*
 * Methods
 *==========================================================================*/

static
void
nci_state_poll_active_handle_ntf(
    NciState* state,
    guint8 gid,
    guint8 oid,
    const GUtilData* payload)
{
    switch (gid) {
    case NCI_GID_CORE:
        switch (oid) {
        case NCI_OID_CORE_INTERFACE_ERROR:
            if (nci_state_poll_active_interface_error_ntf(state, payload)) {
                return;
            }
            break;
        }
        break;
    case NCI_GID_RF:
        switch (oid) {
        case NCI_OID_RF_DEACTIVATE:
            nci_sm_handle_rf_deactivate_ntf(nci_state_sm(state), payload);
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
nci_state_poll_active_init(
    NciStatePollActive* self)
{
}

static
void
nci_state_poll_active_class_init(
    NciStatePollActiveClass* klass)
{
    NCI_STATE_CLASS(klass)->handle_ntf = nci_state_poll_active_handle_ntf;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
