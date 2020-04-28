/*
 * Copyright (C) 2020 Jolla Ltd.
 * Copyright (C) 2020 Slava Monich <slava.monich@jolla.com>
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

typedef NciState NciStateListenActive;
typedef NciStateClass NciStateListenActiveClass;

GType nci_state_listen_active_get_type(void) NCI_INTERNAL;
#define THIS_TYPE (nci_state_listen_active_get_type())
#define PARENT_CLASS (nci_state_listen_active_parent_class)

G_DEFINE_TYPE(NciStateListenActive, nci_state_listen_active, NCI_TYPE_STATE)

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
gboolean
nci_state_listen_active_interface_error_ntf(
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
     * 5.2.6 State RFST_LISTEN_ACTIVE
     *
     * ...
     * When using the ISO-DEP or NFC-DEP RF interface, and the NFCC
     * detects an error during the RF communication, which does not
     * require returning to the IDLE state, as defined in the Listen
     * Mode state machine, the NFCC SHALL send CORE_INTERFACE_ERROR_NTF,
     * using the appropriate status out of RF_TRANSMISSION_ERROR,
     * RF_PROTOCOL_ERROR and RF_TIMEOUT_ERROR. The state will then
     * remain RFST_LISTEN_ACTIVE.
     */
    if (len == 2) {
        NciSm* sm = nci_state_sm(self);

        switch (pkt[0]) {
        case NCI_RF_TRANSMISSION_ERROR:
            GDEBUG("CORE_INTERFACE_ERROR_NTF (Transmission Error)");
            nci_sm_switch_to(sm, NCI_RFST_DISCOVERY);
            return TRUE;
        case NCI_RF_PROTOCOL_ERROR:
            GDEBUG("CORE_INTERFACE_ERROR_NTF (Protocol Error)");
            nci_sm_switch_to(sm, NCI_RFST_DISCOVERY);
            return TRUE;
        case NCI_RF_TIMEOUT_ERROR:
            GDEBUG("CORE_INTERFACE_ERROR_NTF (Timeout)");
            nci_sm_switch_to(sm, NCI_RFST_DISCOVERY);
            return TRUE;
        }
    }
    /* Unrecognized notification */
    return FALSE;
}

static
void
nci_state_listen_active_rf_deactivate_ntf(
    NciState* self,
    const GUtilData* payload)
{
    NciSm* sm = nci_state_sm(self);

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
        switch ((NCI_DEACTIVATION_TYPE)payload->bytes[0]) {
        case NCI_DEACTIVATE_TYPE_SLEEP:
            GDEBUG("RF_DEACTIVATE_NTF Sleep (%u)", payload->bytes[1]);
            nci_sm_enter_state(sm, NCI_RFST_LISTEN_SLEEP, NULL);
            return;
        case NCI_DEACTIVATE_TYPE_SLEEP_AF:
            GDEBUG("RF_DEACTIVATE_NTF Sleep_AF (%u)", payload->bytes[1]);
            nci_sm_enter_state(sm, NCI_RFST_LISTEN_SLEEP, NULL);
            return;
        case NCI_DEACTIVATE_TYPE_IDLE:
        case NCI_DEACTIVATE_TYPE_DISCOVERY:
            break;
        }
    }
    /* Default handling (transition to IDLE or DISCOVERY) */
    nci_sm_handle_rf_deactivate_ntf(sm, payload);
}

/*==========================================================================*
 * Interface
 *==========================================================================*/

NciState*
nci_state_listen_active_new(
    NciSm* sm)
{
    NciState* self = g_object_new(THIS_TYPE, NULL);

    nci_state_init_base(self, sm, NCI_RFST_LISTEN_ACTIVE, "RFST_LISTEN_ACTIVE");
    return self;
}

/*==========================================================================*
 * Methods
 *==========================================================================*/

static
void
nci_state_listen_active_handle_ntf(
    NciState* self,
    guint8 gid,
    guint8 oid,
    const GUtilData* payload)
{
    switch (gid) {
    case NCI_GID_CORE:
        switch (oid) {
        case NCI_OID_CORE_INTERFACE_ERROR:
            if (nci_state_listen_active_interface_error_ntf(self, payload)) {
                return;
            }
            break;
        }
        break;
    case NCI_GID_RF:
        switch (oid) {
        case NCI_OID_RF_DEACTIVATE:
            nci_state_listen_active_rf_deactivate_ntf(self, payload);
            return;
        }
        break;
    }
    NCI_STATE_CLASS(PARENT_CLASS)->handle_ntf(self, gid, oid, payload);
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
void
nci_state_listen_active_init(
    NciStateListenActive* self)
{
}

static
void
nci_state_listen_active_class_init(
    NciStateListenActiveClass* klass)
{
    NCI_STATE_CLASS(klass)->handle_ntf = nci_state_listen_active_handle_ntf;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
