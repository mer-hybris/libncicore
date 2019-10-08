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
#include "nci_state_impl.h"
#include "nci_param_w4_all_discoveries.h"
#include "nci_util.h"
#include "nci_log.h"

typedef NciState NciStateDiscovery;
typedef NciStateClass NciStateDiscoveryClass;

G_DEFINE_TYPE(NciStateDiscovery, nci_state_discovery, NCI_TYPE_STATE)
#define THIS_TYPE (nci_state_discovery_get_type())
#define PARENT_CLASS (nci_state_discovery_parent_class)

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
void
nci_state_discovery_intf_activated_ntf(
    NciState* self,
    const GUtilData* payload)
{
    NciIntfActivationNtf ntf;
    NciModeParam mode_param;
    NciActivationParam activation_param;
    NciSm* sm = nci_state_sm(self);

    /*
     * 5.2.2 State RFST_DISCOVERY
     *
     * ...
     * While polling, if the NFCC discovers just one Remote NFC Endpoint
     * that supports just one Protocol, the NFCC SHALL try to automatically
     * activate it. The NFCC SHALL first establish any underlying protocol(s)
     * with the Remote NFC Endpoint that are needed by the configured RF
     * Interface. On completion, the NFCC SHALL activate the RF Interface
     * and send RF_INTF_ACTIVATED_NTF (Poll Mode) to the DH. At this point,
     * the state is changed to RFST_POLL_ACTIVE.
     */
    if (nci_parse_intf_activated_ntf(&ntf, &mode_param, &activation_param,
        payload->bytes, payload->size)) {
        nci_sm_intf_activated(sm, &ntf);
        nci_sm_enter_state(sm, NCI_RFST_POLL_ACTIVE, NULL);
    } else {
        /* Deactivate this target */
        nci_sm_enter_state(sm, NCI_RFST_POLL_ACTIVE, NULL);
        nci_sm_switch_to(sm, NCI_RFST_DISCOVERY);
    }
}

static
void
nci_state_discovery_discover_ntf(
    NciState* self,
    const GUtilData* payload)
{
    NciDiscoveryNtf ntf;
    NciModeParam param;

    /*
     * 5.2.2 State RFST_DISCOVERY
     *
     * ...
     * While polling, if the NFCC discovers more than one Remote NFC Endpoint,
     * or a Remote NFC Endpoint that supports more than one RF Protocol, it
     * SHALL start sending RF_DISCOVER_NTF messages to the DH. At this point,
     * the state is changed to RFST_W4_ALL_DISCOVERIES.
     */
    if (nci_parse_discover_ntf(&ntf, &param, payload->bytes, payload->size)) {
        NciSm* sm = nci_state_sm(self);
        NciParam* param = NCI_PARAM(nci_param_w4_all_discoveries_new(&ntf));

        nci_sm_enter_state(sm, NCI_RFST_W4_ALL_DISCOVERIES, param);
        nci_param_unref(param);
    }
}

static
gboolean
nci_state_discovery_generic_error_ntf(
    NciState* self,
    const GUtilData* payload)
{
    const guint8* pkt = payload->bytes;
    const guint len = payload->size;

    /*
     * Table 18: Control Messages for Generic Error
     *
     * CORE_GENERIC_ERROR_NTF
     *
     * +=========================================================+
     * | Offset | Size | Description                             |
     * +=========================================================+
     * | 0      | 1    | Status                                  |
     * +=========================================================+
     *
     * 5.2.2 State RFST_DISCOVERY
     *
     * ...
     * If the protocol activation is not successful, the NFCC SHALL
     * send CORE_GENERIC_ERROR_NTF to the DH with status
     * DISCOVERY_TARGET_ACTIVATION_FAILED and stay in RFST_DISCOVERY.
     *
     * In this state, the NFCC MAY detect a command during the RF
     * communication, which forces it to come back to the IDLE state,
     * as defined in the [NFC Forum Activity Technical Specification]
     * Listen Mode state machine. If the RF Protocol deactivation is
     * completed, the NFCC SHALL send CORE_GENERIC_ERROR_NTF
     * (DISCOVERY_TEAR_DOWN), and the state will remain RFST_DISCOVERY.
     */
    if (len == 1) {
        switch (pkt[0]) {
        case NCI_DISCOVERY_TARGET_ACTIVATION_FAILED:
            GDEBUG("CORE_GENERIC_ERROR_NTF (Activation Failed)");
            return TRUE;
        case NCI_DISCOVERY_TEAR_DOWN:
            GDEBUG("CORE_GENERIC_ERROR_NTF (Tear Down)");
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
nci_state_discovery_new(
    NciSm* sm)
{
    NciState* self = g_object_new(THIS_TYPE, NULL);

    nci_state_init_base(self, sm, NCI_RFST_DISCOVERY, "RFST_DISCOVERY");
    return self;
}

/*==========================================================================*
 * Methods
 *==========================================================================*/

static
void
nci_state_discovery_handle_ntf(
    NciState* self,
    guint8 gid,
    guint8 oid,
    const GUtilData* payload)
{
    switch (gid) {
    case NCI_GID_CORE:
        switch (oid) {
        case NCI_OID_CORE_GENERIC_ERROR:
            if (nci_state_discovery_generic_error_ntf(self, payload)) {
                return;
            }
        }
        break;
    case NCI_GID_RF:
        switch (oid) {
        case NCI_OID_RF_DISCOVER:
            nci_state_discovery_discover_ntf(self, payload);
            return;
        case NCI_OID_RF_INTF_ACTIVATED:
            nci_state_discovery_intf_activated_ntf(self, payload);
            return;
        case NCI_OID_RF_DEACTIVATE:
            nci_sm_handle_rf_deactivate_ntf(nci_state_sm(self), payload);
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
nci_state_discovery_init(
    NciStateDiscovery* self)
{
}

static
void
nci_state_discovery_class_init(
    NciStateDiscoveryClass* klass)
{
    NCI_STATE_CLASS(klass)->handle_ntf = nci_state_discovery_handle_ntf;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
