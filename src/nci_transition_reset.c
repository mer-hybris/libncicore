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
#include "nci_sar.h"
#include "nci_sm.h"
#include "nci_log.h"

typedef NciTransition NciTransitionReset;
typedef NciTransitionClass NciTransitionResetClass;

G_DEFINE_TYPE(NciTransitionReset, nci_transition_reset, NCI_TYPE_TRANSITION)
#define THIS_TYPE (nci_transition_reset_get_type())
#define PARENT_CLASS (nci_transition_reset_parent_class)

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
void
nci_transition_reset_set_config_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* self)
{
    NciSm* sm = nci_transition_sm(self);

    if (status == NCI_REQUEST_CANCELLED || !nci_transition_active(self)) {
        GDEBUG("CORE_SET_CONFIG cancelled");
        return;
    } else if (status == NCI_REQUEST_TIMEOUT) {
        GDEBUG("CORE_SET_CONFIG timed out");
    } else if (sm) {
        /*
         * Table 10: Control Messages for Setting Configuration Parameters
         *
         * CORE_SET_CONFIG_RSP
         *
         * +=========================================================+
         * | Offset | Size | Description                             |
         * +=========================================================+
         * | 0      | 1    | Status                                  |
         * | 1      | 1    | The number of invalid Parameters (n)    |
         * | 2      | n    | Invalid parameters                      |
         * +=========================================================+
         */
        if (status == NCI_REQUEST_SUCCESS &&
            payload->size >= 2 &&
            payload->bytes[0] == NCI_STATUS_OK) {
            GDEBUG("%c CORE_SET_CONFIG_RSP ok", DIR_IN);
        } else {
            GWARN("CORE_SET_CONFIG_CMD failed (continuing anyway)");
        }
        nci_transition_finish(self, NULL);
        return;
    }
    nci_sm_stall(sm, NCI_STALL_ERROR);
}

static
void
nci_transition_reset_set_config(
    NciTransition* self)
{
    /*
     * Table 10: Control Messages for Setting Configuration Parameters
     *
     * CORE_SET_CONFIG_CMD
     *
     * +=========================================================+
     * | Offset | Size | Description                             |
     * +=========================================================+
     * | 0      | 1    | The number of Parameter fields (n)      |
     * | 1      | ...  | Parameters * n                          |
     * |        |      +-----------------------------------------+
     * |        |      | ID  | 1 | The identifier                |
     * |        |      | Len | 1 | The length of Val (m)         |
     * |        |      | Val | m | The value of the parameter    |
     * +=========================================================+
     */
    static const guint8 cmd[] = {
        2,
        NCI_CONFIG_PA_BAIL_OUT, 0x01, 0x00,
        NCI_CONFIG_PB_BAIL_OUT, 0x01, 0x00
    };

    GDEBUG("%c CORE_SET_CONFIG_CMD", DIR_OUT);
    nci_transition_send_command_static(self,
        NCI_GID_CORE, NCI_OID_CORE_SET_CONFIG, cmd, sizeof(cmd),
        nci_transition_reset_set_config_rsp);
}

static
void
nci_transition_reset_get_config_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* self)
{
    NciSm* sm = nci_transition_sm(self);

    if (status == NCI_REQUEST_CANCELLED || !nci_transition_active(self)) {
        GDEBUG("CORE_GET_CONFIG cancelled");
        return;
    } else if (status == NCI_REQUEST_TIMEOUT) {
        GDEBUG("CORE_GET_CONFIG timed out");
    } else if (sm) {
        /*
         * Table 11: Control Messages for Reading Current Configuration
         *
         * CORE_GET_CONFIG_RSP
         *
         * +=========================================================+
         * | Offset | Size | Description                             |
         * +=========================================================+
         * | 0      | 1    | Status                                  |
         * | 1      | 1    | The number of Parameters (n)            |
         * | 2      | ...  | Parameters                              |
         * |        |      +-----------------------------------------+
         * |        |      | ID  | 1 | The identifier                |
         * |        |      | Len | 1 | The length of Val (m)         |
         * |        |      | Val | m | The value of the parameter    |
         * +=========================================================+
         */
        if (status == NCI_REQUEST_SUCCESS && payload->size >= 2) {
            const guint8* pkt = payload->bytes;
            const guint len = payload->size;
            const guint n = pkt[1];

            if (pkt[0] == NCI_STATUS_OK) {
                GDEBUG("%c CORE_GET_CONFIG_RSP ok", DIR_IN);
            } else if (pkt[0] == NCI_STATUS_INVALID_PARAM && len >= 2 + n*2) {
#if GUTIL_LOG_DEBUG
                /*
                 * [NFCForum-TS-NCI-1.0]
                 * 4.3.2 Retrieve the Configuration
                 *
                 * If the DH tries to retrieve any parameter(s) that
                 * are not available in the NFCC, the NFCC SHALL respond
                 * with a CORE_GET_CONFIG_RSP with a Status field of
                 * STATUS_INVALID_PARAM, containing each unavailable
                 * Parameter ID with a Parameter Len field of value zero.
                 * In this case, the CORE_GET_CONFIG_RSP SHALL NOT include
                 * any parameter(s) that are available on the NFCC.
                 */
                if (GLOG_ENABLED(GLOG_LEVEL_DEBUG)) {
                    GString* buf = g_string_new(NULL);
                    guint i;

                    for (i = 0; i < n; i++) {
                        g_string_append_printf(buf, " %02x", pkt[2 + i*2]);
                    }
                    GDEBUG("%c CORE_GET_CONFIG_RSP invalid parameter(s):%s",
                        DIR_IN, buf->str);
                    g_string_free(buf, TRUE);
                }
#endif
            } else {
                GWARN("CORE_GET_CONFIG_CMD error 0x%02x (continuing anyway)",
                    payload->bytes[0]);
            }
            nci_transition_reset_set_config(self);
            return;
        } else {
            GWARN("CORE_GET_CONFIG_CMD failed (continuing anyway)");
        }
    }
    nci_sm_stall(sm, NCI_STALL_ERROR);
}

static
void
nci_transition_reset_get_config(
    NciTransition* self)
{
    /*
     * Table 11: Control Messages for Reading Current Configuration
     *
     * CORE_GET_CONFIG_CMD
     *
     * +=========================================================+
     * | Offset | Size | Description                             |
     * +=========================================================+
     * | 0      | 1    | The number of Parameter ID fields (n)   |
     * | 1      | n    | Parameter ID                            |
     * +=========================================================+
     */
    static const guint8 cmd[] = {
        2,
        NCI_CONFIG_PA_BAIL_OUT,
        NCI_CONFIG_PB_BAIL_OUT
    };

    /*
     * We may want to set some parameters some day but for now let's just
     * query something and see how it works...
     */
    GDEBUG("%c CORE_GET_CONFIG_CMD", DIR_OUT);
    nci_transition_send_command_static(self,
        NCI_GID_CORE, NCI_OID_CORE_GET_CONFIG, cmd, sizeof(cmd),
        nci_transition_reset_get_config_rsp);
}

static
void
nci_transition_reset_init_v1_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* self)
{
    NciSm* sm = nci_transition_sm(self);
    NciSar* sar = nci_sm_sar(sm);

    if (status == NCI_REQUEST_CANCELLED || !nci_transition_active(self)) {
        GDEBUG("%c CORE_INIT (v1) cancelled", DIR_IN);
    } else if (status == NCI_REQUEST_TIMEOUT) {
        GDEBUG("%c CORE_INIT (v1) timed out", DIR_IN);
        nci_sm_stall(sm, NCI_STALL_ERROR);
    } else if (sar && status == NCI_REQUEST_SUCCESS) {
        const guint8* pkt = payload->bytes;
        const guint len = payload->size;
        guint n; /* Number of Supported RF Interfaces */

        /*
         * NFC Controller Interface (NCI), Version 1.1, Section 4.2
         *
         * CORE_INIT_RSP
         *
         * +=========================================================+
         * | Offset | Size | Description                             |
         * +=========================================================+
         * | 0      | 1    | Status                                  |
         * | 1      | 4    | NFCC Features                           |
         * | 5      | 1    | Number of Supported RF Interfaces (n)   |
         * | 6      | n    | Supported RF Interfaces                 |
         * | 6 + n  | 1    | Max Logical Connections                 |
         * | 7 + n  | 2    | Max Routing Table Size                  |
         * | 9 + n  | 1    | Max Control Packet Payload Size         |
         * | 10 + n | 2    | Max Size for Large Parameters           |
         * | 12 + n | 1    | Manufacturer ID                         |
         * | 13 + n | 4    | Manufacturer Specific Information       |
         * +=========================================================+
         */
        if (len >= 17 && pkt[0] == NCI_STATUS_OK &&
            len == ((n = pkt[5]) + 17)) {
            const guint8* rf_interfaces = pkt + 6;
            guint8 max_logical_conns = pkt[6 + n];
            guint8 max_control_packet = pkt[9 + n];

            if (sm->rf_interfaces) {
                g_bytes_unref(sm->rf_interfaces);
                sm->rf_interfaces = NULL;
            }
            if (n > 0) {
                sm->rf_interfaces = g_bytes_new(rf_interfaces, n);
            }

            sm->nfcc_discovery = pkt[1];
            sm->nfcc_routing = pkt[2];
            sm->nfcc_power = pkt[3];
            sm->max_routing_table_size = ((guint)pkt[8 + n] << 8) + pkt[7 + n];

            GDEBUG("%c CORE_INIT_RSP (v1) ok", DIR_IN);
            GDEBUG("  Features = %02x %02x %02x %02x",
                pkt[1], pkt[2], pkt[3], pkt[4]);
#if GUTIL_LOG_DEBUG
            if (GLOG_ENABLED(GLOG_LEVEL_DEBUG)) {
                GString* buf = g_string_new(NULL);
                guint i;

                for (i = 0; i < n; i++) {
                    g_string_append_printf(buf, " %02x", rf_interfaces[i]);
                }
                GDEBUG("  Supported interfaces =%s", buf->str);
                g_string_free(buf, TRUE);
            }
#endif
            GDEBUG("  Max Logical Connections = %u", max_logical_conns);
            GDEBUG("  Max Routing Table Size = %u", sm->max_routing_table_size);
            GDEBUG("  Max Control Packet Size = %u", max_control_packet);
            GDEBUG("  Manufacturer = 0x%02x", pkt[12 + n]);
            GDEBUG("  Manufacturer Info = %02x %02x %02x %02x",
                pkt[13 + n], pkt[14 + n], pkt[15 + n], pkt[16 + n]);

            nci_sar_set_max_logical_connections(sar, max_logical_conns);
            nci_sar_set_max_control_packet_size(sar, max_control_packet);
            nci_transition_reset_get_config(self);
            return;
        }
        GWARN("CORE_INIT (v1) failed (or is incomprehensible)");
        nci_sm_stall(sm, NCI_STALL_ERROR);
    }
}

static
void
nci_transition_reset_init_v2_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* self)
{
    NciSm* sm = nci_transition_sm(self);
    NciSar* sar = nci_sm_sar(sm);

    if (status == NCI_REQUEST_CANCELLED || !nci_transition_active(self)) {
        GDEBUG("CORE_INIT (v2) cancelled");
        return;
    } else if (status == NCI_REQUEST_TIMEOUT) {
        GDEBUG("CORE_INIT (v2) timed out");
    } else if (sar && status == NCI_REQUEST_SUCCESS) {
        const guint8* pkt = payload->bytes;
        const guint len = payload->size;
        guint n; /* Number of Supported RF Interfaces */

        /*
         * NFC Controller Interface (NCI), Version 2.0, Section 4.2
         *
         * CORE_INIT_RSP
         *
         * +=========================================================+
         * | Offset | Size | Description                             |
         * +=========================================================+
         * | 0      | 1    | Status                                  |
         * | 1      | 4    | NFCC Features                           |
         * | 5      | 1    | Max Logical Connections                 |
         * | 6      | 2    | Max Routing Table Size                  |
         * | 8      | 1    | Max Control Packet Payload Size         |
         * | 9      | 1    | Max Static HCI Packet Size              |
         * | 10     | 1    | Number of Static HCI Connection Credits |
         * | 11     | 2    | Max NFC-V RF Frame Size                 |
         * | 13     | 1    | Number of Supported RF Interfaces (n)   |
         * | 14     | 2*n  | Supported RF Interfaces and Extensions  |
         * +=========================================================+
         */
        if (len >= 14 && pkt[0] == NCI_STATUS_OK &&
            len == (2 * (n = pkt[13]) + 14)) {
            const guint8* rf_interfaces = pkt + 14;
            guint8 max_logical_conns = pkt[5];
            guint8 max_control_packet = pkt[8];

            if (sm->rf_interfaces) {
                g_bytes_unref(sm->rf_interfaces);
                sm->rf_interfaces = NULL;
            }
            if (n > 0) {
                sm->rf_interfaces = g_bytes_new(rf_interfaces, n);
            }

            sm->nfcc_discovery = pkt[1];
            sm->nfcc_routing = pkt[2];
            sm->nfcc_power = pkt[3];
            sm->max_routing_table_size = ((guint)pkt[7] << 8) + pkt[6];

            GDEBUG("%c CORE_INIT_RSP (v2) ok", DIR_IN);
            GDEBUG("  Features = %02x %02x %02x %02x",
                pkt[1], pkt[2], pkt[3], pkt[4]);
#if GUTIL_LOG_DEBUG
            if (GLOG_ENABLED(GLOG_LEVEL_DEBUG)) {
                GString* buf = g_string_new(NULL);
                guint i;

                for (i = 0; i < n; i++) {
                    g_string_append_printf(buf, " %02x", rf_interfaces[2 * i]);
                }
                GDEBUG("  Supported interfaces =%s", buf->str);
                g_string_free(buf, TRUE);
            }
#endif
            GDEBUG("  Max Logical Connections = %u", max_logical_conns);
            GDEBUG("  Max Routing Table Size = %u", sm->max_routing_table_size);
            GDEBUG("  Max Control Packet Size = %u", max_control_packet);

            nci_sar_set_max_logical_connections(sar, max_logical_conns);
            nci_sar_set_max_control_packet_size(sar, max_control_packet);
            nci_transition_reset_get_config(self);
            return;
        }
        GWARN("CORE_INIT (v1) failed (or is incomprehensible)");
    }
    nci_sm_stall(sm, NCI_STALL_ERROR);
}

static
void
nci_transition_reset_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* self)
{
    NciSm* sm = nci_transition_sm(self);

    if (status == NCI_REQUEST_CANCELLED || !nci_transition_active(self)) {
        GDEBUG("CORE_RESET cancelled");
        return;
    } else if (status == NCI_REQUEST_TIMEOUT) {
        GDEBUG("CORE_RESET timed out");
    } else if (sm && status == NCI_REQUEST_SUCCESS) {
        const guint8* pkt = payload->bytes;
        const guint len = payload->size;

        /*
         * Table 5: Control Messages to Reset the NFCC
         *
         * CORE_RESET_RSP
         *
         * +=========================================================+
         * | Offset | Size | Description                             |
         * +=========================================================+
         * | 0      | 1    | Status                                  |
         * | 1      | 1    | NCI Version (0x10 == 1.0)               |
         * | 2      | 1    | Configuration Status                    |
         * +=========================================================+
         */
        if (len == 3) {
            sm->version = NCI_INTERFACE_VERSION_1;
            if (pkt[0] == NCI_STATUS_OK) {
                GDEBUG("%c CORE_RESET_RSP (v1) ok", DIR_IN);
                GDEBUG("%c CORE_INIT_CMD (v1)", DIR_OUT);
                nci_transition_send_command(self, NCI_GID_CORE,
                    NCI_OID_CORE_INIT, NULL, nci_transition_reset_init_v1_rsp);
                return;
            }
            GWARN("CORE_RESET_CMD failed");
        } else if (len == 1) {
            sm->version = NCI_INTERFACE_VERSION_2;
            if (pkt[0] == NCI_STATUS_OK) {
                /* Wait for notification */
                GDEBUG("%c CORE_RESET_RSP (v2) ok", DIR_IN);
                return;
            }
            GWARN("CORE_RESET_CMD (v2) failed");
        } else {
            GWARN("Unexpected CORE_RESET_RSP length %u byte(s)", len);
        }
    }
    nci_sm_stall(sm, NCI_STALL_ERROR);
}

/*==========================================================================*
 * Interface
 *==========================================================================*/

NciTransition* 
nci_transition_reset_new(
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
nci_transition_reset_handle_ntf(
    NciTransition* self,
    guint8 gid,
    guint8 oid,
    const GUtilData* payload)
{
    NciSm* sm = nci_transition_sm(self);

    switch (gid) {
    case NCI_GID_CORE:
        switch (oid) {
        case NCI_OID_CORE_RESET:
            /* Notification is only expected in NCI 2.x case */
            if (sm && sm->version == NCI_INTERFACE_VERSION_2) {
                static const guint8 cmd[] = { 0x00, 0x00 };

                GDEBUG("CORE_RESET_NTF (v2)");
                GDEBUG("%c CORE_INIT_CMD (v2)", DIR_OUT);
                nci_transition_send_command_static(self,
                    NCI_GID_CORE, NCI_OID_CORE_INIT, cmd, sizeof(cmd),
                    nci_transition_reset_init_v2_rsp);
                return;
            }
        }
        break;
    }
    NCI_TRANSITION_CLASS(PARENT_CLASS)->handle_ntf(self, gid, oid, payload);
}

static
gboolean
nci_transition_reset_start(
    NciTransition* self)
{
    NciSm* sm = nci_transition_sm(self);

    if (sm) {
        /*
         * Table 5: Control Messages to Reset the NFCC
         *
         * CORE_RESET_CMD
         *
         * +=========================================================+
         * | Offset | Size | Description                             |
         * +=========================================================+
         * | 0      | 1    | Reset Type  | 0 | Keep Configuration    |
         * |        |      |             | 1 | Reset Configuration   |
         * +=========================================================+
         */
        static const guint8 cmd[] = { 0x00 /* Keep Configuration */ };

        /* Reset the state */
        if (sm->rf_interfaces) {
            g_bytes_unref(sm->rf_interfaces);
            sm->rf_interfaces = NULL;
        }
        sm->max_routing_table_size = 0;
        sm->version = NCI_INTERFACE_VERSION_UNKNOWN;
        sm->nfcc_discovery = NCI_NFCC_DISCOVERY_NONE;
        sm->nfcc_routing = NCI_NFCC_ROUTING_NONE;
        sm->nfcc_power = NCI_NFCC_POWER_NONE;

        GDEBUG("%c CORE_RESET_CMD", DIR_OUT);
        return nci_transition_send_command_static(self,
            NCI_GID_CORE, NCI_OID_CORE_RESET, cmd, sizeof(cmd),
            nci_transition_reset_rsp);
    }
    return FALSE;
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
void
nci_transition_reset_init(
    NciTransitionReset* self)
{
}

static
void
nci_transition_reset_class_init(
    NciTransitionResetClass* klass)
{
    klass->start = nci_transition_reset_start;
    klass->handle_ntf = nci_transition_reset_handle_ntf;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
