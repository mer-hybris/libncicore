/*
 * Copyright (C) 2019-2021 Jolla Ltd.
 * Copyright (C) 2019-2021 Slava Monich <slava.monich@jolla.com>
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
#include "nci_util_p.h"
#include "nci_sm.h"
#include "nci_log.h"

/*==========================================================================*
 * NCI_RFST_IDLE -> NCI_RFST_DISCOVERY transition
 *
 * +---------------------+
 * | CORE_GET_CONFIG_CMD |
 * | 1. LA_SEL_INFO      |
 * | 2. LF_PROTOCOL_TYPE |
 * +---------------------+
 *      |            |
 *    error          ok
 *      |            |
 *      |            v
 *      |   +--------------------+
 *      |   | All parameters OK? |------------\
 *      |   +--------------------+            |
 *      |            |                        |
 *      |           yes                       no
 *      |            |                        |
 *      v            v                        v
 * +---------------------------+       +----------------+
 * | Are listen modes enabled? |<------| SET_CONFIG_CMD |
 * +---------------------------+       +----------------+
 *      |                  |
 *      no                yes
 *      |                  |
 *      |                  v
 *      |  +------------------------------------+
 *      |  | What kind of routing is supported? |
 *      |  +------------------------------------+
 *      |     |     |      |               |
 *      |     |     |   protocol          all
 *      |     |     |      |               |
 *      |     | technology |               v
 *      |     |     |      |  +----------------------------+
 *      |    none   |      |  | RF_SET_LISTEN_MODE_ROUTING |-------------\
 *      |     |     |      |  |          (Mixed)           |             |
 *      |     |     |      |  +----------------------------+             ok
 *      |     |     |      |               |                             |
 *      |     |     |      |             error                           |
 *      |     |     |      |               |                             |
 *      |     |     |      |               v                             |
 *      |     |     |      |  +----------------------------+             |
 *      |     |     |      |  | RF_SET_LISTEN_MODE_ROUTING |             |
 *      |     |     |      |  |        (Protocol)          |------\      |
 *      |     |     |      |  +----------------------------+      |      |
 *      |     |     |      v                            |         ok     |
 *      |     |     |  +----------------------------+   |         |      |
 *      |     |     |  | RF_SET_LISTEN_MODE_ROUTING |   |         |      |
 *      |     |     |  |        (Protocol)          |   |         |      |
 *      |     |     |  +----------------------------+   |         |      |
 *      |     |     v                             |   error       |      |
 *      |     |  +----------------------------+   |     |         |      |
 *      |     |  | RF_SET_LISTEN_MODE_ROUTING |<--|-----/         |      |
 *      |     |  |        (Technology)        |   |               |      |
 *      |     |  +----------------------------+   |               |      |
 *      |     |                             |     |               /      |
 *      |     \--------------------------\  |     |              /       |
 *      v                                |  |     |             /        |
 *  +------------------------------+     |  |     |            /         |
 *  | Is routing supported at all? |     |  |     |           /          |
 *  +------------------------------+     |  |     |          /           |
 *               |                \      |  |     |         /            |
 *              yes                no    |  |     |        /             |
 *               |                  \    |  |     |       /              |
 *               v                   \   |  |     |      /               |
 *  +----------------------------+    v  v  v     v     v                |
 *  | RF_SET_LISTEN_MODE_ROUTING |   +---------------------+             |
 *  |        (default)           |-->| RF_DISCOVER_MAP_CMD |<------------/
 *  +----------------------------+   +---------------------+
 *                                            |
 *                                            v
 *                                    +-----------------+
 *                                    | RF_DISCOVER_CMD |
 *                                    +-----------------+
 *==========================================================================*/

typedef NciTransition NciTransitionIdleToDiscovery;
typedef NciTransitionClass NciTransitionIdleToDiscoveryClass;

GType nci_transition_idle_to_discovery_get_type(void) NCI_INTERNAL;
#define THIS_TYPE (nci_transition_idle_to_discovery_get_type())
#define PARENT_CLASS (nci_transition_idle_to_discovery_parent_class)

G_DEFINE_TYPE(NciTransitionIdleToDiscovery, nci_transition_idle_to_discovery,
    NCI_TYPE_TRANSITION)

#define ARRAY_AND_SIZE(a) a, sizeof(a)

/*==========================================================================*
 * Implementation
 *==========================================================================*/

static
gboolean
nci_transition_idle_to_discovery_send_byte_array(
    NciTransition* self,
    guint8 gid,
    guint8 oid,
    GByteArray* cmd,
    NciTransitionResponseFunc resp)
{
    GBytes* bytes = g_byte_array_free_to_bytes(cmd);
    gboolean ret = nci_transition_send_command(self, gid, oid, bytes, resp);

    g_bytes_unref(bytes);
    return ret;
}

static
void
nci_transition_idle_to_discovery_discover_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* self)
{
    if (status == NCI_REQUEST_CANCELLED || !nci_transition_active(self)) {
        GDEBUG("RF_DISCOVER_MAP cancelled");
    } else if (status == NCI_REQUEST_TIMEOUT) {
        GDEBUG("RF_DISCOVER_MAP timed out");
        nci_transition_stall(self, NCI_STALL_ERROR);
    } else {
        const guint8* pkt = payload->bytes;
        const guint len = payload->size;

        /*
         * Table 52: Control Messages to Start Discovery
         *
         * RF_DISCOVER_RSP
         *
         * +=========================================================+
         * | Offset | Size | Description                             |
         * +=========================================================+
         * | 0      | 1    | Status                                  |
         * +=========================================================+
         */
        if (len > 0 && pkt[0] == NCI_STATUS_OK) {
            GDEBUG("%c RF_DISCOVER_RSP ok", DIR_IN);
            nci_transition_finish(self, NULL);
        } else {
            if (len > 0) {
                GWARN("%c RF_DISCOVER_RSP error %u", DIR_IN, pkt[0]);
            } else {
                GWARN("%c Broken RF_DISCOVER_RSP", DIR_IN);
            }
            nci_transition_stall(self, NCI_STALL_ERROR);
        }
    }
}

static
void
nci_transition_idle_to_discovery_discover(
    NciTransition* self)
{
    NciSm* sm = nci_transition_sm(self);
    GByteArray* cmd = g_byte_array_sized_new(9);
    const gboolean peer_poll = (sm->op_mode &
        (NFC_OP_MODE_PEER|NFC_OP_MODE_POLL)) ==
        (NFC_OP_MODE_PEER|NFC_OP_MODE_POLL);
    const gboolean peer_listen = (sm->op_mode &
        (NFC_OP_MODE_PEER|NFC_OP_MODE_LISTEN)) ==
        (NFC_OP_MODE_PEER|NFC_OP_MODE_LISTEN);
    const gboolean ce_listen = (sm->op_mode &
        (NFC_OP_MODE_CE|NFC_OP_MODE_LISTEN)) ==
        (NFC_OP_MODE_CE|NFC_OP_MODE_LISTEN);

    /*
     * Table 52: Control Messages to Start Discovery
     *
     * RF_DISCOVER_CMD
     *
     * +=========================================================+
     * | Offset | Size | Description                             |
     * +=========================================================+
     * | 0      | 1    | Number of Configurations                |
     * | 1      | 2*n  | Configurations:                         |
     * |        |      +-----------------------------------------+
     * |        |      | 0 | RF Technology and Mode              |
     * |        |      | 1 | Frequency (1 = every period)        |
     * +=========================================================+
     */

    static const guint8 cmd_header[] = {
        0x00, /* Number of Configurations */
    };

    GDEBUG("%c RF_DISCOVER_CMD", DIR_OUT);
    g_byte_array_append(cmd, ARRAY_AND_SIZE(cmd_header));

    /*
     * RW Modes: Poll A/B/F/V
     * Peer Modes: Poll/Listen A/F
     * CE Modes: Listen A/B
     */

    /* Poll side */
    if ((sm->op_mode & NFC_OP_MODE_RW) || peer_poll) {
        static const guint8 poll_ab[] = {
            NCI_MODE_ACTIVE_POLL_A, 1,
            NCI_MODE_PASSIVE_POLL_A, 1,
            NCI_MODE_PASSIVE_POLL_B, 1,
        };

        GDEBUG("  ActivePollA");
        GDEBUG("  PassivePollA");
        GDEBUG("  PassivePollB");
        cmd->data[0] += 3; /* Number of entries */
        g_byte_array_append(cmd, ARRAY_AND_SIZE(poll_ab));

        if (sm->options & NCI_OPTION_POLL_F) {
            static const guint8 poll_f[] = {
                NCI_MODE_ACTIVE_POLL_F, 1,
                NCI_MODE_PASSIVE_POLL_F, 1
            };

            /* NFC-DEP or T3T */
            GDEBUG("  ActivePollF");
            GDEBUG("  PassivePollF");
            cmd->data[0] += 2; /* Number of entries */
            g_byte_array_append(cmd, ARRAY_AND_SIZE(poll_f));
        }

        if (sm->options & NCI_OPTION_POLL_V) {
            static const guint8 poll_v[] = {
                NCI_MODE_PASSIVE_POLL_V, 1
            };

            GDEBUG("  PassivePollV");
            cmd->data[0] += 1; /* Number of entries */
            g_byte_array_append(cmd, ARRAY_AND_SIZE(poll_v));
        }
    }

    /* Listen side */
    if (peer_listen || ce_listen) {
        static const guint8 listen_ab[] = {
            NCI_MODE_PASSIVE_LISTEN_A, 1
        };

        GDEBUG("  PassiveListenA");
        cmd->data[0] += 1; /* Number of entries */
        g_byte_array_append(cmd, ARRAY_AND_SIZE(listen_ab));
    }
    if (ce_listen) {
        static const guint8 listen_ab[] = {
            NCI_MODE_ACTIVE_LISTEN_A, 1,
            NCI_MODE_PASSIVE_LISTEN_B, 1
        };

        GDEBUG("  ActiveListenA");
        GDEBUG("  PassiveListenB");
        cmd->data[0] += 2; /* Number of entries */
        g_byte_array_append(cmd, ARRAY_AND_SIZE(listen_ab));
    }
    if (peer_listen && (sm->options & NCI_OPTION_LISTEN_F)) {
        static const guint8 listen_f[] = {
            NCI_MODE_PASSIVE_LISTEN_F, 1
        };

        GDEBUG("  PassiveListenF");
        cmd->data[0] += 1; /* Number of entries */
        g_byte_array_append(cmd, ARRAY_AND_SIZE(listen_f));
    }

    nci_transition_idle_to_discovery_send_byte_array(self,
        NCI_GID_RF, NCI_OID_RF_DISCOVER, cmd,
        nci_transition_idle_to_discovery_discover_rsp);
}

static
void
nci_transition_idle_to_discover_map_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* self)
{
    if (status == NCI_REQUEST_CANCELLED || !nci_transition_active(self)) {
        GDEBUG("RF_DISCOVER_MAP cancelled");
    } else if (status == NCI_REQUEST_TIMEOUT) {
        GDEBUG("RF_DISCOVER_MAP timed out");
        nci_transition_stall(self, NCI_STALL_ERROR);
    } else {
        const guint8* pkt = payload->bytes;
        const guint len = payload->size;

        /*
         * Table 42: Control Messages for RF Interface Mapping Configuration
         *
         * RF_DISCOVER_MAP_RSP
         *
         * +=========================================================+
         * | Offset | Size | Description                             |
         * +=========================================================+
         * | 0      | 1    | Status                                  |
         * +=========================================================+
         */
        if (len > 0 && pkt[0] == NCI_STATUS_OK) {
            GDEBUG("%c RF_DISCOVER_MAP_RSP ok", DIR_IN);
            nci_transition_idle_to_discovery_discover(self);
        } else {
            if (len > 0) {
                GWARN("%c RF_DISCOVER_MAP_RSP error %u", DIR_IN, pkt[0]);
            } else {
                GWARN("%c Broken RF_DISCOVER_MAP_RSP", DIR_IN);
            }
            nci_transition_stall(self, NCI_STALL_ERROR);
        }
    }
}

static
gboolean
nci_transition_idle_to_discover_map(
    NciTransition* self)
{
    NciSm* sm = nci_transition_sm(self);
    GByteArray* cmd = g_byte_array_sized_new(22);

    /*
     * Table 42: Control Messages for RF Interface Mapping Configuration
     *
     * RF_DISCOVER_MAP_CMD
     *
     * +=========================================================+
     * | Offset | Size | Description                             |
     * +=========================================================+
     * | 0      | 1    | Number of Mapping Configurations (n)    |
     * | 1      | 3*n  | Mapping Configurations:                 |
     * |        |      +-----------------------------------------+
     * |        |      | 0 | RF Protocol                         |
     * |        |      | 1 | Mode                                |
     * |        |      | 2 | RF Interface                        |
     * +=========================================================+
     */

    static const guint8 cmd_header[] = {
        0x00, /* Number of Mapping Configurations */
    };

    GDEBUG("%c RF_DISCOVER_MAP_CMD", DIR_OUT);
    g_byte_array_append(cmd, ARRAY_AND_SIZE(cmd_header));
    if (sm->op_mode & NFC_OP_MODE_RW) {
        static const guint8 entries[] = {
            NCI_PROTOCOL_T1T,
            NCI_DISCOVER_MAP_MODE_POLL,
            NCI_RF_INTERFACE_FRAME,

            NCI_PROTOCOL_T2T,
            NCI_DISCOVER_MAP_MODE_POLL,
            NCI_RF_INTERFACE_FRAME,

            NCI_PROTOCOL_ISO_DEP,
            NCI_DISCOVER_MAP_MODE_POLL,
            NCI_RF_INTERFACE_ISO_DEP
        };

        GDEBUG("  T1T/Poll/Frame");
        GDEBUG("  T2T/Poll/Frame");
        cmd->data[0] += 3; /* Number of entries */
        g_byte_array_append(cmd, ARRAY_AND_SIZE(entries));
        if (sm->options & NCI_OPTION_POLL_F) {
            static const guint8 t3t_entry[] = {
                NCI_PROTOCOL_T3T,
                NCI_DISCOVER_MAP_MODE_POLL,
                NCI_RF_INTERFACE_FRAME
            };

            GDEBUG("  T3T/Poll/Frame");
            cmd->data[0] += 1; /* Number of entries */
            g_byte_array_append(cmd, ARRAY_AND_SIZE(t3t_entry));
        }
        GDEBUG("  IsoDep/Poll/IsoDep");
    }

    if (sm->op_mode & NFC_OP_MODE_PEER) {
        if (sm->op_mode & NFC_OP_MODE_POLL) {
            static const guint8 entries[] = {
                NCI_PROTOCOL_NFC_DEP,
                NCI_DISCOVER_MAP_MODE_POLL,
                NCI_RF_INTERFACE_NFC_DEP
            };

            GDEBUG("  NfcDep/Poll/NfcDep");
            cmd->data[0] += 1; /* Number of entries */
            g_byte_array_append(cmd, ARRAY_AND_SIZE(entries));
        }
        if (sm->op_mode & NFC_OP_MODE_LISTEN) {
            static const guint8 entries[] = {
                NCI_PROTOCOL_NFC_DEP,
                NCI_DISCOVER_MAP_MODE_LISTEN,
                NCI_RF_INTERFACE_NFC_DEP
            };

            GDEBUG("  NfcDep/Listen/NfcDep");
            cmd->data[0] += 1; /* Number of entries */
            g_byte_array_append(cmd, ARRAY_AND_SIZE(entries));
        }
    }

    if (sm->op_mode & NFC_OP_MODE_CE) {
        static const guint8 entries[] = {
            NCI_PROTOCOL_ISO_DEP,
            NCI_DISCOVER_MAP_MODE_LISTEN,
            NCI_RF_INTERFACE_ISO_DEP
        };

        GDEBUG("  IsoDep/Listen/IsoDep");
        cmd->data[0] += 1; /* Number of entries */
        g_byte_array_append(cmd, ARRAY_AND_SIZE(entries));
    }

    return nci_transition_idle_to_discovery_send_byte_array(self,
        NCI_GID_RF, NCI_OID_RF_DISCOVER_MAP, cmd,
        nci_transition_idle_to_discover_map_rsp);
}

static
void
nci_transition_idle_to_discovery_add_routing_entry(
    NciSm* sm,
    GByteArray* cmd,
    const guint8* entry,
    const char* name)
{
    /*
     * Table 44: Control Messages to Configure Listen Mode Routing
     *
     * Routing entry
     *
     * +=========================================================+
     * | Offset | Size | Description                             |
     * +=========================================================+
     * | 0      | 1    | Type   | Defined in Table 46.           |
     * | 1      | 1    | Length | The length of Value (x)        |
     * | 2      | x    | Value  | Value of the Routing TLV       |
     * +=========================================================+
     */
    const guint entry_size = 2 /* type + length */ + entry[1] /* value */;

    /*
     * 6.3.2 Configure Listen Mode Routing
     *
     * ...
     * All parameters except 'More' and 'Number of Routing Entries'
     * are included in the calculation to determine if the routing
     * configuration size exceeds the Max Routing Table Size.
     */
    if (cmd->len <= sm->max_routing_table_size + 2 /* not included  */) {
        cmd->data[1] += 1; /* Number of entries */
        g_byte_array_append(cmd, entry, entry_size);
        GDEBUG("  %s", name);
    } else {
        GDEBUG("  %s (didn't fit)", name);
    }
}

static
void
nci_transition_idle_to_discovery_protocol_routing_entries(
    NciSm* sm,
    GByteArray* cmd)
{
    /*
     * Placing NFC-DEP and ISO-DEP entries to the head of the list
     * gives them higher priority.
     */
    if (sm->op_mode & NFC_OP_MODE_PEER) {
        static const guint8 nfc_dep[] = {
            NCI_ROUTING_ENTRY_TYPE_PROTOCOL,
            3,
            NCI_NFCEE_ID_DH,
            NCI_ROUTING_ENTRY_POWER_ON,
            NCI_PROTOCOL_NFC_DEP
        };

        nci_transition_idle_to_discovery_add_routing_entry
            (sm, cmd, nfc_dep, "NFC-DEP");
    }

    if (sm->op_mode & (NFC_OP_MODE_CE | NFC_OP_MODE_RW)) {
        static const guint8 iso_dep[] = {
            NCI_ROUTING_ENTRY_TYPE_PROTOCOL,
            3,
            NCI_NFCEE_ID_DH,
            NCI_ROUTING_ENTRY_POWER_ON,
            NCI_PROTOCOL_ISO_DEP
        };

        nci_transition_idle_to_discovery_add_routing_entry
            (sm, cmd, iso_dep, "ISO-DEP");
    }

    if (sm->op_mode & NFC_OP_MODE_RW) {
        static const guint8 t1[] = {
            NCI_ROUTING_ENTRY_TYPE_PROTOCOL,
            3,
            NCI_NFCEE_ID_DH,
            NCI_ROUTING_ENTRY_POWER_ON,
            NCI_PROTOCOL_T1T
        };
        static const guint8 t2[] = {
            NCI_ROUTING_ENTRY_TYPE_PROTOCOL,
            3,
            NCI_NFCEE_ID_DH,
            NCI_ROUTING_ENTRY_POWER_ON,
            NCI_PROTOCOL_T2T
        };

        nci_transition_idle_to_discovery_add_routing_entry(sm, cmd, t1, "T1T");
        nci_transition_idle_to_discovery_add_routing_entry(sm, cmd, t2, "T2T");
        if (sm->options & NCI_OPTION_POLL_F) {
            static const guint8 t3[] = {
                NCI_ROUTING_ENTRY_TYPE_PROTOCOL,
                3,
                NCI_NFCEE_ID_DH,
                NCI_ROUTING_ENTRY_POWER_ON,
                NCI_PROTOCOL_T3T
            };

            nci_transition_idle_to_discovery_add_routing_entry
                (sm, cmd, t3, "T3T");
        }
    }
}

static
void
nci_transition_idle_to_discovery_tech_routing_entries(
    NciSm* sm,
    GByteArray* cmd)
{
    /*
     * RW Modes: Poll A/B/F/V
     * Peer Modes: Poll/Listen A/F
     * CE Modes: Listen A/B
     */

    static const guint8 nfca[] = {
        NCI_ROUTING_ENTRY_TYPE_TECHNOLOGY,
        3,
        NCI_NFCEE_ID_DH,
        NCI_ROUTING_ENTRY_POWER_ON,
        NCI_RF_TECHNOLOGY_A
    };

    static const guint8 nfcb[] = {
        NCI_ROUTING_ENTRY_TYPE_TECHNOLOGY,
        3,
        NCI_NFCEE_ID_DH,
        NCI_ROUTING_ENTRY_POWER_ON,
        NCI_RF_TECHNOLOGY_B
    };

    /*
     * Placing NFC-F to the head of the list may (or may not) give it
     * higher priority.
     */
    if ((sm->options & NCI_OPTION_LISTEN_F) &&
        (sm->op_mode & (NFC_OP_MODE_RW|NFC_OP_MODE_PEER))) {
        static const guint8 nfcf[] = {
            NCI_ROUTING_ENTRY_TYPE_TECHNOLOGY,
            3,
            NCI_NFCEE_ID_DH,
            NCI_ROUTING_ENTRY_POWER_ON,
            NCI_RF_TECHNOLOGY_F
        };

        nci_transition_idle_to_discovery_add_routing_entry
            (sm, cmd, nfcf, "NFC-F");
    }

    if ((sm->options & NCI_OPTION_LISTEN_V) &&
        (sm->op_mode & NFC_OP_MODE_RW)) {
        static const guint8 nfcv[] = {
            NCI_ROUTING_ENTRY_TYPE_TECHNOLOGY,
            3,
            NCI_NFCEE_ID_DH,
            NCI_ROUTING_ENTRY_POWER_ON,
            NCI_RF_TECHNOLOGY_V
        };

        nci_transition_idle_to_discovery_add_routing_entry
            (sm, cmd, nfcv, "NFC-V");
    }

    nci_transition_idle_to_discovery_add_routing_entry(sm, cmd, nfcb, "NFC-B");
    nci_transition_idle_to_discovery_add_routing_entry(sm, cmd, nfca, "NFC-A");
}

static
void
nci_transition_idle_to_discovery_mixed_routing_entries(
    NciSm* sm,
    GByteArray* cmd)
{
    nci_transition_idle_to_discovery_protocol_routing_entries(sm, cmd);
    nci_transition_idle_to_discovery_tech_routing_entries(sm, cmd);
}

static
void
nci_transition_idle_to_discovery_default_routing_entries(
    NciSm* sm,
    GByteArray* cmd)
{
    static const guint8 nfca[] = {
        NCI_ROUTING_ENTRY_TYPE_TECHNOLOGY,
        3,
        NCI_NFCEE_ID_DH,
        NCI_ROUTING_ENTRY_POWER_ON,
        NCI_RF_TECHNOLOGY_A
    };

    /*
     * [NFCForum-TS-NCI-1.0]
     * 6.3.2 Configure Listen Mode Routing
     *
     * ...
     * This Control Message SHALL be at least one Routing Entry.
     */
    nci_transition_idle_to_discovery_add_routing_entry(sm, cmd, nfca, "NFC-A");
}

static
void
nci_transition_idle_to_discovery_last_set_routing_rsp(
    NciTransition* self,
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    const char* name)
{
    if (status == NCI_REQUEST_CANCELLED || !nci_transition_active(self)) {
        GDEBUG("%s cancelled", name);
    } else if (status == NCI_REQUEST_TIMEOUT) {
        GDEBUG("%s timed out", name);
        nci_transition_stall(self, NCI_STALL_ERROR);
    } else {
        const guint8* pkt = payload->bytes;
        const guint len = payload->size;

        if (len > 0 && pkt[0] == NCI_STATUS_OK) {
            GDEBUG("%c %s ok", DIR_IN, name);
        } else if (len > 0) {
            GDEBUG("%c %s error %u", DIR_IN, name, pkt[0]);
        } else {
            GDEBUG("%c Broken %s", DIR_IN, name);
        }
        /* Ignore errors */
        nci_transition_idle_to_discover_map(self);
    }
}

static
void
nci_transition_idle_to_discovery_set_default_routing_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* self)
{
    nci_transition_idle_to_discovery_last_set_routing_rsp(self, status,
        payload, "RF_SET_LISTEN_MODE_ROUTING (Default)");
}

static
void
nci_transition_idle_to_discovery_set_tech_routing_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* self)
{
    nci_transition_idle_to_discovery_last_set_routing_rsp(self, status,
        payload, "RF_SET_LISTEN_MODE_ROUTING (Technology)");
}

static
void
nci_transition_idle_to_discovery_last_protocol_routing_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* self)
{
    nci_transition_idle_to_discovery_last_set_routing_rsp(self, status,
        payload, "RF_SET_LISTEN_MODE_ROUTING (Protocol)");
}

static
void
nci_transition_idle_to_discovery_set_routing(
    NciTransition* self,
    const char* name,
    void (*add_routing_entries)(NciSm* sm, GByteArray* cmd),
    NciTransitionResponseFunc rsp)
{
    GByteArray* cmd = g_byte_array_sized_new(64);

    /*
     * Table 44: Control Messages to Configure Listen Mode Routing
     *
     * RF_SET_LISTEN_MODE_ROUTING_CMD
     *
     * +=========================================================+
     * | Offset | Size | Description                             |
     * +=========================================================+
     * | 0      | 1    | More   | 0 | Last Message               |
     * |        |      |        | 1 | More Message(s) to follow  |
     * |        |      +-----------------------------------------+
     * | 1      | 1    | The number of Routing Entries (n)       |
     * | 2      | n*x  | Routing Entries                         |
     * +=========================================================+
     */

    static const guint8 cmd_header[] = {
        0x00, /* Last message */
        0x00  /* Number of Routing Entries */
    };

    GDEBUG("%c RF_SET_LISTEN_MODE_ROUTING_CMD (%s)", DIR_OUT, name);
    g_byte_array_append(cmd, ARRAY_AND_SIZE(cmd_header));
    add_routing_entries(nci_transition_sm(self), cmd);

    /* Takes ownership of the cmd array */
    nci_transition_idle_to_discovery_send_byte_array(self,
        NCI_GID_RF, NCI_OID_RF_SET_LISTEN_MODE_ROUTING, cmd, rsp);
}

static
void
nci_transition_idle_to_discovery_set_protocol_routing_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* self)
{
    NciSm* sm = nci_transition_sm(self);
#if GUTIL_LOG_DEBUG
    static const char cmd[] = "RF_SET_LISTEN_MODE_ROUTING (Protocol)";
#endif
    if (status == NCI_REQUEST_CANCELLED || !nci_transition_active(self)) {
        GDEBUG("%s cancelled", cmd);
    } else if (status == NCI_REQUEST_TIMEOUT) {
        GDEBUG("%s timed out", cmd);
        nci_sm_stall(sm, NCI_STALL_ERROR);
    } else if (sm) {
        const guint8* pkt = payload->bytes;
        const guint len = payload->size;

        if (len > 0 && pkt[0] == NCI_STATUS_OK) {
            GDEBUG("%c %s ok", DIR_IN, cmd);
            nci_transition_idle_to_discover_map(self);
        } else {
            if (len > 0) {
                GDEBUG("%c %s error %u", DIR_IN, cmd, pkt[0]);
            } else {
                GDEBUG("%c Broken %s", DIR_IN, cmd);
            }
            /* Try technology based routing */
            nci_transition_idle_to_discovery_set_routing(self, "Technology",
                nci_transition_idle_to_discovery_tech_routing_entries,
                nci_transition_idle_to_discovery_set_tech_routing_rsp);
        }
    }
}

static
void
nci_transition_idle_to_discovery_set_mixed_routing_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* self)
{
    NciSm* sm = nci_transition_sm(self);
#if GUTIL_LOG_DEBUG
    static const char cmd[] = "RF_SET_LISTEN_MODE_ROUTING (Mixed)";
#endif
    if (status == NCI_REQUEST_CANCELLED || !nci_transition_active(self)) {
        GDEBUG("%s cancelled", cmd);
    } else if (status == NCI_REQUEST_TIMEOUT) {
        GDEBUG("%s timed out", cmd);
        nci_sm_stall(sm, NCI_STALL_ERROR);
    } else if (sm) {
        const guint8* pkt = payload->bytes;
        const guint len = payload->size;

        if (len > 0 && pkt[0] == NCI_STATUS_OK) {
            GDEBUG("%c %s ok", DIR_IN, cmd);
            nci_transition_idle_to_discover_map(self);
        } else {
            if (len > 0) {
                GDEBUG("%c %s error %u", DIR_IN, cmd, pkt[0]);
            } else {
                GDEBUG("%c Broken %s", DIR_IN, cmd);
            }
            /* Try protocol based routing */
            nci_transition_idle_to_discovery_set_routing(self, "Protocol",
                nci_transition_idle_to_discovery_protocol_routing_entries,
                nci_transition_idle_to_discovery_set_protocol_routing_rsp);
        }
    }
}

static
void
nci_transition_idle_to_discovery_configure_routing(
    NciTransition* self)
{
    NciSm* sm = nci_transition_sm(self);

    if (sm->max_routing_table_size) {
        if (sm->op_mode & NFC_OP_MODE_LISTEN) {
            /* Listen modes enabled */
            switch (sm->nfcc_routing &
                (NCI_NFCC_ROUTING_PROTOCOL_BASED |
                 NCI_NFCC_ROUTING_TECHNOLOGY_BASED)) {
            case NCI_NFCC_ROUTING_PROTOCOL_BASED |
                 NCI_NFCC_ROUTING_TECHNOLOGY_BASED:
                nci_transition_idle_to_discovery_set_routing(self, "Mixed",
                    nci_transition_idle_to_discovery_mixed_routing_entries,
                    nci_transition_idle_to_discovery_set_mixed_routing_rsp);
                return;
            case NCI_NFCC_ROUTING_PROTOCOL_BASED:
                nci_transition_idle_to_discovery_set_routing(self, "Protocol",
                    nci_transition_idle_to_discovery_protocol_routing_entries,
                    nci_transition_idle_to_discovery_last_protocol_routing_rsp);
                return;
            case NCI_NFCC_ROUTING_TECHNOLOGY_BASED:
                nci_transition_idle_to_discovery_set_routing(self, "Technology",
                    nci_transition_idle_to_discovery_tech_routing_entries,
                    nci_transition_idle_to_discovery_set_tech_routing_rsp);
                return;
            }
        } else {
            /* Listen modes disabled */
            nci_transition_idle_to_discovery_set_routing(self, "Default",
                nci_transition_idle_to_discovery_default_routing_entries,
                nci_transition_idle_to_discovery_set_default_routing_rsp);
            return;
        }
    }
    nci_transition_idle_to_discover_map(self);
}

static
void
nci_transition_idle_to_discovery_set_config_rsp(
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
        nci_transition_idle_to_discovery_configure_routing(self);
        return;
    }
    nci_sm_stall(sm, NCI_STALL_ERROR);
}

static
void
nci_transition_idle_to_discovery_set_config(
    NciTransition* self,
    guint8 la_sel_info,
    guint8 lf_protocol_type)
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
    const gsize len = 7;
    guint8* cmd = g_new(guint8, len);
    GBytes* bytes = g_bytes_new_take(cmd, len);

    cmd[0] = 2;
    cmd[1] = NCI_CONFIG_LA_SEL_INFO;
    cmd[2] = 1;
    cmd[3] = la_sel_info;
    cmd[4] = NCI_CONFIG_LF_PROTOCOL_TYPE;
    cmd[5] = 1;
    cmd[6] = lf_protocol_type;

    GDEBUG("%c CORE_SET_CONFIG_CMD (LA_SEL_INFO,LF_PROTOCOL_TYPE)", DIR_OUT);
    nci_transition_send_command(self, NCI_GID_CORE, NCI_OID_CORE_SET_CONFIG,
        bytes, nci_transition_idle_to_discovery_set_config_rsp);
    g_bytes_unref(bytes);
}

static
gboolean
nci_transition_idle_to_discovery_config_byte_ok(
    guint nparams,
    const GUtilData* params,
    guint8 id,
    const char* name,
    guint8* expected,
    guint8 mask)
{
    guint value;

    if (nci_parse_config_param_uint(nparams, params, id, &value) == 1) {
        const guint8 byte = (guint8)value;

        if ((byte & mask) == *expected) {
            GDEBUG("%s 0x%02x is ok", name, value);
            return TRUE;
        } else {
            *expected |= byte & ~mask;
            GDEBUG("%s 0x%02x needs to be 0x%02x", name, byte, *expected);
        }
    } else {
        GDEBUG("Could not determine %s", name);
    }
    return FALSE;
}

static
guint8
nci_transition_idle_to_discovery_la_sel_info_expected(
    NciSm* sm)
{
    guint expected = 0;

    if ((sm->op_mode & (NFC_OP_MODE_LISTEN | NFC_OP_MODE_CE)) ==
        (NFC_OP_MODE_LISTEN | NFC_OP_MODE_CE)) {
        /* Need ISO-DEP in listen mode */
        expected |= NCI_LA_SEL_INFO_ISO_DEP;
    }
    if ((sm->op_mode & NFC_OP_MODE_PEER) == NFC_OP_MODE_PEER) {
        /* Need NFC-DEP in listen mode */
        expected |= NCI_LA_SEL_INFO_NFC_DEP;
    }
    return expected;
}

static
gboolean
nci_transition_idle_to_discovery_la_sel_info_ok(
    guint nparams,
    const GUtilData* params,
    guint8* expected)
{
    return nci_transition_idle_to_discovery_config_byte_ok(nparams, params,
        NCI_CONFIG_LA_SEL_INFO, "LA_SEL_INFO", expected,
        NCI_LA_SEL_INFO_ISO_DEP | NCI_LA_SEL_INFO_NFC_DEP);
}

static
guint8
nci_transition_idle_to_discovery_lf_protocol_type_expected(
    NciSm* sm)
{
    /*
     * [NFCForum-TS-NCI-1.0]
     *
     * 6.1.8 Listen F Parameters
     *
     * ...
     * If the DH is interested in NFC-DEP based communication it SHALL
     * set the b1 of LF_PROTOCOL_TYPE to 1, which will enables the
     * generation of SENSF_RES indicating NFC-DEP capabilities as a
     * response to a SENSF_REQ having a System Code of FFFFh. Otherwise
     * the DH SHALL set the b1 of this field to 0.
     */
    if ((sm->op_mode & NFC_OP_MODE_PEER) == NFC_OP_MODE_PEER &&
        (sm->options & NCI_OPTION_LISTEN_F)) {
        /* Need NFC-DEP in Listen F mode */
        return NCI_LF_PROTOCOL_TYPE_NFC_DEP;
    } else {
        return 0;
    }
}

static
gboolean
nci_transition_idle_to_discovery_lf_protocol_type_ok(
    guint nparams,
    const GUtilData* params,
    guint8* expected)
{
    return nci_transition_idle_to_discovery_config_byte_ok(nparams, params,
        NCI_CONFIG_LF_PROTOCOL_TYPE, "LF_PROTOCOL_TYPE", expected,
        NCI_LF_PROTOCOL_TYPE_NFC_DEP);
}

static
void
nci_transition_idle_to_discovery_get_config_rsp(
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
        nci_sm_stall(sm, NCI_STALL_ERROR);
    } else if (sm) {
        guint8 la_sel_info =
            nci_transition_idle_to_discovery_la_sel_info_expected(sm);
        guint8 lf_protocol_type =
            nci_transition_idle_to_discovery_lf_protocol_type_expected(sm);

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
            const guint cmd_status = payload->bytes[0];
            const guint n = payload->bytes[1];
            GUtilData data;

            data.bytes = payload->bytes + 2;
            data.size = payload->size - 2;
            if (cmd_status == NCI_STATUS_OK) {
                gboolean la_sel_info_ok, lf_protocol_type_ok;

                GDEBUG("%c CORE_GET_CONFIG_RSP ok", DIR_IN);
                la_sel_info_ok =
                    nci_transition_idle_to_discovery_la_sel_info_ok
                        (n,  &data, &la_sel_info);
                lf_protocol_type_ok =
                    nci_transition_idle_to_discovery_lf_protocol_type_ok
                        (n,  &data, &lf_protocol_type);
                if (la_sel_info_ok && lf_protocol_type_ok) {
                    /* No need to set parameters */
                    nci_transition_idle_to_discovery_configure_routing(self);
                    return;
                }
            } else if (cmd_status == NCI_STATUS_INVALID_PARAM) {
                NCI_DUMP_INVALID_CONFIG_PARAMS(n, &data);
            } else {
                GWARN("CORE_GET_CONFIG_CMD error 0x%02x (continuing anyway)",
                    cmd_status);
            }
        } else {
            GWARN("CORE_GET_CONFIG_CMD unexpected response");
        }
        nci_transition_idle_to_discovery_set_config(self,
            la_sel_info, lf_protocol_type);
    }
}

/*==========================================================================*
 * Interface
 *==========================================================================*/

NciTransition* 
nci_transition_idle_to_discovery_new(
    NciSm* sm)
{
    NciState* dest = nci_sm_get_state(sm, NCI_RFST_DISCOVERY);

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
nci_transition_idle_to_discovery_start(
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
        NCI_CONFIG_LA_SEL_INFO,
        NCI_CONFIG_LF_PROTOCOL_TYPE
    };

    GDEBUG("%c CORE_GET_CONFIG_CMD (LA_SEL_INFO,LF_PROTOCOL_TYPE)", DIR_OUT);
    return nci_transition_send_command_static(self,
        NCI_GID_CORE, NCI_OID_CORE_GET_CONFIG, cmd, sizeof(cmd),
        nci_transition_idle_to_discovery_get_config_rsp);
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
void
nci_transition_idle_to_discovery_init(
    NciTransitionIdleToDiscovery* self)
{
}

static
void
nci_transition_idle_to_discovery_class_init(
    NciTransitionIdleToDiscoveryClass* klass)
{
    klass->start = nci_transition_idle_to_discovery_start;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
