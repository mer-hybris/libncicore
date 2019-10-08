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
#include "nci_param_w4_host_select.h"
#include "nci_util.h"
#include "nci_log.h"

typedef NciStateClass NciStateW4HostSelectClass;
typedef NciState NciStateW4HostSelect;

G_DEFINE_TYPE(NciStateW4HostSelect, nci_state_w4_host_select, NCI_TYPE_STATE)
#define THIS_TYPE (nci_state_w4_host_select_get_type())
#define PARENT_CLASS (nci_state_w4_host_select_parent_class)
#define NCI_STATE_W4_HOST_SELECT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        THIS_TYPE, NciStateW4HostSelect))

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
gint
nci_state_w4_host_select_sort(
    gconstpointer a,
    gconstpointer b)
{
    const NciDiscoveryNtf* ntf_a = a;
    const NciDiscoveryNtf* ntf_b = b;

    /*
     * Return 0 if elements are equal, a negative value if the
     * first element comes before the second, or a positive value if
     * the first element comes after the second.
     */
    if (ntf_a->protocol != ntf_b->protocol) {
        static const NCI_PROTOCOL protocol_order[] = {
            NCI_PROTOCOL_T2T,
            NCI_PROTOCOL_ISO_DEP
        };
        guint i;

        for (i = 0; i < G_N_ELEMENTS(protocol_order); i++) {
            if (ntf_a->protocol == protocol_order[i]) {
                return -1;
            } else if (ntf_b->protocol == protocol_order[i]) {
                return 1;
            }
        }
    }

    /* Otherwise sort by discovery ID */
    return (gint)(ntf_a->discovery_id) - (gint)(ntf_b->discovery_id);
}

static
void
nci_state_w4_host_select_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    gpointer user_data)
{
    if (status == NCI_REQUEST_SUCCESS) {
        const guint8* pkt = payload->bytes;
        const guint len = payload->size;

        /*
         * Table 60: Control Messages to select a Discovered Target
         *
         * RF_DISCOVER_SELECT_RSP
         *
         * +=========================================================+
         * | Offset | Size | Description                             |
         * +=========================================================+
         * | 0      | 1    | Status                                  |
         * +=========================================================+
         */
        if (len > 0 && pkt[0] == NCI_STATUS_OK) {
            GDEBUG("%c RF_DISCOVER_SELECT_RSP ok", DIR_IN);
        } else {
            if (len > 0) {
                GWARN("%c RF_DISCOVER_SELECT_RSP error %u", DIR_IN, pkt[0]);
            } else {
                GWARN("%c Broken RF_DISCOVER_SELECT_RSP", DIR_IN);
            }
        }
    } else {
        GWARN("RF_DISCOVER_SELECT failed");
    }
}

static
void
nci_state_w4_host_select_entered(
    NciStateW4HostSelect* self,
    NciParam* param)
{
    if (NCI_IS_PARAM_W4_HOST_SELECT(param)) {
        NciSm* sm = nci_state_sm(self);
        NciParamW4HostSelect* select = NCI_PARAM_W4_HOST_SELECT(param);
        GSList* selected = NULL;
        guint i;

        /* Select a supported protocol */
        for (i = 0; i < select->count; i++) {
            NciDiscoveryNtf* ntf = select->ntf + i;

            if (nci_sm_supports_protocol(sm, ntf->protocol)) {
                selected = g_slist_insert_sorted(selected, ntf,
                    nci_state_w4_host_select_sort);
            }
        }

        /*
         * We may want to store the list and selected the next protocol
         * if the best one gets rejected.
         */
        if (selected) {
            const NciDiscoveryNtf* ntf = selected->data;
            GBytes* payload;

            /*
             * Table 60: Control Messages to select a Discovered Target
             *
             * RF_DISCOVER_SELECT_CMD
             *
             * +=========================================================+
             * | Offset | Size | Description                             |
             * +=========================================================+
             * | 0      | 1    | RF Discovery ID                         |
             * | 1      | 1    | RF Protocol                             |
             * | 2      | 1    | RF Interface                            |
             * +=========================================================+
             */
            guint8 cmd[3];
            cmd[0] = ntf->discovery_id;
            cmd[1] = ntf->protocol;
            cmd[2] = (ntf->protocol == NCI_PROTOCOL_ISO_DEP) ?
                NCI_RF_INTERFACE_ISO_DEP :
                NCI_RF_INTERFACE_FRAME;

            GDEBUG("%c RF_DISCOVER_SELECT_CMD (0x%02x)", DIR_OUT,
                ntf->discovery_id);
            payload = g_bytes_new(cmd, sizeof(cmd));
            nci_sm_send_command(sm, NCI_GID_RF, NCI_OID_RF_DISCOVER_SELECT,
                payload, nci_state_w4_host_select_rsp, self);
            g_bytes_unref(payload);
            g_slist_free(selected);
        }
    }
}

static
void
nci_state_w4_host_select_handle_intf_activated_ntf(
    NciState* self,
    const GUtilData* payload)
{
    NciIntfActivationNtf ntf;
    NciModeParam mode_param;
    NciActivationParam activation_param;
    NciSm* sm = nci_state_sm(self);

    /*
     * 5.2.4 State RFST_W4_HOST_SELECT
     *
     * ...
     * When the DH sends RF_DISCOVER_SELECT_CMD with a valid RF Discovery
     * ID, RF Protocol and RF Interface, the NFCC SHALL try to activate
     * the associated Remote NFC Endpoint (depending on the state of the
     * Remote NFC Endpoint). The NFCC SHALL first establish any underlying
     * protocol(s) with the Remote NFC Endpoint that are needed by the
     * configured RF Interface. On completion, the NFCC SHALL activate
     * the RF Interface and send RF_INTF_ACTIVATED_NTF (Poll Mode) to
     * the DH. At this point, the state is changed to RFST_POLL_ACTIVE.
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
gboolean
nci_state_w4_host_select_generic_error_ntf(
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
     * 5.2.4 State RFST_W4_HOST_SELECT
     *
     * ...
     * If the activation was not successful, the NFCC SHALL
     * send CORE_GENERIC_ERROR_NTF to the DH with a Status of
     * DISCOVERY_TARGET_ACTIVATION_FAILED and the state will
     * remain as RFST_W4_HOST_SELECT.
     */
    if (len == 1) {
        switch (pkt[0]) {
        case NCI_DISCOVERY_TARGET_ACTIVATION_FAILED:
            GDEBUG("CORE_GENERIC_ERROR_NTF (Activation Failed)");
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
nci_state_w4_host_select_new(
    NciSm* sm)
{
    NciState* self = g_object_new(THIS_TYPE, NULL);

    nci_state_init_base(self, sm, NCI_RFST_W4_HOST_SELECT,
        "RFST_W4_HOST_SELECT");
    return self;
}

/*==========================================================================*
 * Methods
 *==========================================================================*/

static
void
nci_state_w4_host_select_enter(
    NciState* self,
    NciParam* param)
{
    nci_state_w4_host_select_entered(self, param);
    NCI_STATE_CLASS(PARENT_CLASS)->enter(self, param);
}

static
void
nci_state_w4_host_select_reenter(
    NciState* self,
    NciParam* param)
{
    nci_state_w4_host_select_entered(self, param);
    NCI_STATE_CLASS(PARENT_CLASS)->reenter(self, param);
}

static
void
nci_state_w4_host_select_handle_ntf(
    NciState* self,
    guint8 gid,
    guint8 oid,
    const GUtilData* payload)
{
    switch (gid) {
    case NCI_GID_CORE:
        switch (oid) {
        case NCI_OID_CORE_GENERIC_ERROR:
            if (nci_state_w4_host_select_generic_error_ntf(self, payload)) {
                return;
            }
        }
        break;
    case NCI_GID_RF:
        switch (oid) {
        case NCI_OID_RF_INTF_ACTIVATED:
            nci_state_w4_host_select_handle_intf_activated_ntf(self, payload);
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
nci_state_w4_host_select_init(
    NciStateW4HostSelect* self)
{
}

static
void
nci_state_w4_host_select_class_init(
    NciStateW4HostSelectClass* klass)
{
    klass->enter = nci_state_w4_host_select_enter;
    klass->reenter = nci_state_w4_host_select_reenter;
    klass->handle_ntf = nci_state_w4_host_select_handle_ntf;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
