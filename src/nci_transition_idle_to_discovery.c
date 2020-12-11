/*
 * Copyright (C) 2019-2020 Jolla Ltd.
 * Copyright (C) 2019-2020 Slava Monich <slava.monich@jolla.com>
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
 * NCI_RFST_IDLE -> NCI_RFST_DISCOVERY transition
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
     * RW Modes: Poll A/B/V
     * Peer Modes: Poll/Listen A/F
     * CE Modes: Listen A/B
     */
    if (sm->op_mode & NFC_OP_MODE_RW) {
        static const guint8 b_entry[] = {
            NCI_MODE_PASSIVE_POLL_B, 1
        };

        GDEBUG("  PassivePollB");
        cmd->data[0] += 1; /* Number of entries */
        g_byte_array_append(cmd, ARRAY_AND_SIZE(b_entry));
        if (sm->options & NCI_OPTION_POLL_V) {
            static const guint8 v_entry[] = {
                NCI_MODE_PASSIVE_POLL_V, 1
            };

            GDEBUG("  PassivePollV");
            cmd->data[0] += 1; /* Number of entries */
            g_byte_array_append(cmd, ARRAY_AND_SIZE(v_entry));
        }
    }
    if ((sm->op_mode & NFC_OP_MODE_RW) ||
        (sm->op_mode & (NFC_OP_MODE_PEER|NFC_OP_MODE_POLL)) ==
                       (NFC_OP_MODE_PEER|NFC_OP_MODE_POLL)) {
        static const guint8 entries[] = {
            NCI_MODE_PASSIVE_POLL_A, 1,
            NCI_MODE_ACTIVE_POLL_A, 1
        };

        GDEBUG("  PassivePollA");
        GDEBUG("  ActivePollA");
        cmd->data[0] += 2; /* Number of entries */
        g_byte_array_append(cmd, ARRAY_AND_SIZE(entries));
    }
    if ((sm->options & NCI_OPTION_POLL_F) &&
        (sm->op_mode & (NFC_OP_MODE_PEER|NFC_OP_MODE_POLL)) ==
                       (NFC_OP_MODE_PEER|NFC_OP_MODE_POLL)) {
        static const guint8 entries[] = {
            NCI_MODE_PASSIVE_POLL_F, 1,
            NCI_MODE_ACTIVE_POLL_F, 1
        };

        GDEBUG("  PassivePollF");
        GDEBUG("  ActivePollF");
        cmd->data[0] += 2; /* Number of entries */
        g_byte_array_append(cmd, ARRAY_AND_SIZE(entries));
    }
    if ((sm->options & NCI_OPTION_LISTEN_F) &&
        (sm->op_mode & (NFC_OP_MODE_PEER|NFC_OP_MODE_LISTEN)) ==
                       (NFC_OP_MODE_PEER|NFC_OP_MODE_LISTEN)) {
        static const guint8 entries[] = {
            NCI_MODE_PASSIVE_LISTEN_F, 1,
            NCI_MODE_ACTIVE_LISTEN_F, 1
        };

        GDEBUG("  PassiveListenF");
        GDEBUG("  ActiveListenF");
        cmd->data[0] += 2; /* Number of entries */
        g_byte_array_append(cmd, ARRAY_AND_SIZE(entries));
    }
    if ((sm->op_mode & NFC_OP_MODE_CE) ||
        (sm->op_mode & (NFC_OP_MODE_PEER|NFC_OP_MODE_LISTEN)) ==
                       (NFC_OP_MODE_PEER|NFC_OP_MODE_LISTEN)) {
        static const guint8 entries[] = {
            NCI_MODE_PASSIVE_LISTEN_A, 1,
            NCI_MODE_ACTIVE_LISTEN_A, 1
        };

        GDEBUG("  PassiveListenA");
        GDEBUG("  ActiveListenA");
        cmd->data[0] += 2; /* Number of entries */
        g_byte_array_append(cmd, ARRAY_AND_SIZE(entries));
    }
    if (sm->op_mode & NFC_OP_MODE_CE) {
        static const guint8 entries[] = {
            NCI_MODE_PASSIVE_LISTEN_B, 1
        };

        GDEBUG("  PassiveListenB");
        cmd->data[0] += 1; /* Number of entries */
        g_byte_array_append(cmd, ARRAY_AND_SIZE(entries));
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
nci_transition_idle_to_discovery_set_protocol_routing_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* self)
{
    NciSm* sm = nci_transition_sm(self);

    if (status == NCI_REQUEST_CANCELLED || !nci_transition_active(self)) {
        GDEBUG("RF_SET_LISTEN_MODE_ROUTING (Protocol) cancelled");
    } else if (status == NCI_REQUEST_TIMEOUT) {
        GDEBUG("RF_SET_LISTEN_MODE_ROUTING (Protocol) timed out");
        nci_sm_stall(sm, NCI_STALL_ERROR);
    } else if (sm) {
        const guint8* pkt = payload->bytes;
        const guint len = payload->size;

        if (len > 0 && pkt[0] == NCI_STATUS_OK) {
            GDEBUG("%c RF_SET_LISTEN_MODE_ROUTING_RSP (Protocol) ok", DIR_IN);
        } else {
            if (len > 0) {
                GDEBUG("%c RF_SET_LISTEN_MODE_ROUTING_RSP (Protocol) error %u",
                    DIR_IN, pkt[0]);
            } else {
                GDEBUG("%c Broken RF_SET_LISTEN_MODE_ROUTING_RSP (Protocol)",
                    DIR_IN);
            }
            /* Ignore errors */
        }
        nci_transition_idle_to_discover_map(self);
    }
}

static
gboolean
nci_transition_idle_to_discovery_set_protocol_routing(
    NciTransition* self)
{
    NciSm* sm = nci_transition_sm(self);
    GByteArray* cmd = g_byte_array_sized_new(26);

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

    GDEBUG("%c RF_SET_LISTEN_MODE_ROUTING_CMD (Protocol)", DIR_OUT);
    g_byte_array_append(cmd, ARRAY_AND_SIZE(cmd_header));
    if (sm->op_mode & NFC_OP_MODE_RW) {
        static const guint8 entries[] = {
            NCI_ROUTING_ENTRY_TYPE_PROTOCOL,
            3,
            NCI_NFCEE_ID_DH,
            NCI_ROUTING_ENTRY_POWER_ON,
            NCI_PROTOCOL_T1T,

            NCI_ROUTING_ENTRY_TYPE_PROTOCOL,
            3,
            NCI_NFCEE_ID_DH,
            NCI_ROUTING_ENTRY_POWER_ON,
            NCI_PROTOCOL_T2T,

            NCI_ROUTING_ENTRY_TYPE_PROTOCOL,
            3,
            NCI_NFCEE_ID_DH,
            NCI_ROUTING_ENTRY_POWER_ON,
            NCI_PROTOCOL_ISO_DEP
        };

        GDEBUG("  T1T");
        GDEBUG("  T2T");
        cmd->data[1] += 3; /* Number of entries */
        g_byte_array_append(cmd, ARRAY_AND_SIZE(entries));
        if (sm->options & NCI_OPTION_POLL_F) {
            static const guint8 t3t_entry[] = {
                NCI_ROUTING_ENTRY_TYPE_PROTOCOL,
                3,
                NCI_NFCEE_ID_DH,
                NCI_ROUTING_ENTRY_POWER_ON,
                NCI_PROTOCOL_T3T
            };

            GDEBUG("  T3T");
            cmd->data[1] += 1; /* Number of entries */
            g_byte_array_append(cmd, ARRAY_AND_SIZE(t3t_entry));
        }
        GDEBUG("  ISO-DEP");
    } else if (sm->op_mode & NFC_OP_MODE_CE) {
        static const guint8 entries[] = {
            NCI_ROUTING_ENTRY_TYPE_PROTOCOL,
            3,
            NCI_NFCEE_ID_DH,
            NCI_ROUTING_ENTRY_POWER_ON,
            NCI_PROTOCOL_ISO_DEP
        };

        GDEBUG("  ISO-DEP");
        cmd->data[1] += 1; /* Number of entries */
        g_byte_array_append(cmd, ARRAY_AND_SIZE(entries));
    }

    if (sm->op_mode & NFC_OP_MODE_PEER) {
        static const guint8 entries[] = {
            NCI_ROUTING_ENTRY_TYPE_PROTOCOL,
            3,
            NCI_NFCEE_ID_DH,
            NCI_ROUTING_ENTRY_POWER_ON,
            NCI_PROTOCOL_NFC_DEP
        };

        GDEBUG("  NFC-DEP");
        cmd->data[1] += 1; /* Number of entries */
        g_byte_array_append(cmd, ARRAY_AND_SIZE(entries));
    }

    return nci_transition_idle_to_discovery_send_byte_array(self,
        NCI_GID_RF, NCI_OID_RF_SET_LISTEN_MODE_ROUTING, cmd,
        nci_transition_idle_to_discovery_set_protocol_routing_rsp);
}

static
void
nci_transition_idle_to_discovery_set_tech_routing_rsp(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* self)
{
    if (status == NCI_REQUEST_CANCELLED || !nci_transition_active(self)) {
        GDEBUG("RF_SET_LISTEN_MODE_ROUTING (Technology) cancelled");
    } else if (status == NCI_REQUEST_TIMEOUT) {
        GDEBUG("RF_SET_LISTEN_MODE_ROUTING (Technology) timed out");
        nci_transition_stall(self, NCI_STALL_ERROR);
    } else {
        const guint8* pkt = payload->bytes;
        const guint len = payload->size;

        if (len > 0 && pkt[0] == NCI_STATUS_OK) {
            GDEBUG("%c RF_SET_LISTEN_MODE_ROUTING_RSP (Technology) ok", DIR_IN);
            nci_transition_idle_to_discover_map(self);
        } else {
            NciSm* sm = nci_transition_sm(self);

            if (len > 0) {
                GDEBUG("%c RF_SET_LISTEN_MODE_ROUTING_RSP "
                    "(Technology) error %u", DIR_IN, pkt[0]);
            } else {
                GDEBUG("%c Broken RF_SET_LISTEN_MODE_ROUTING_RSP "
                    "(Technology)", DIR_IN);
            }
            if (sm->nfcc_routing & NCI_NFCC_ROUTING_PROTOCOL_BASED) {
                /* Try protocol based routing */
                nci_transition_idle_to_discovery_set_protocol_routing(self);
            } else {
                nci_transition_idle_to_discover_map(self);
            }
        }
    }
}

static
gboolean
nci_transition_idle_to_discovery_set_tech_routing(
    NciTransition* self)
{
    NciSm* sm = nci_transition_sm(self);
    GByteArray* cmd = g_byte_array_sized_new(22);

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

    static const guint8 static_entries[] = {
        0x00, /* Last message */
        0x02, /* Number of Routing Entries */

        NCI_ROUTING_ENTRY_TYPE_TECHNOLOGY,
        3,
        NCI_NFCEE_ID_DH,
        NCI_ROUTING_ENTRY_POWER_ON,
        NCI_RF_TECHNOLOGY_A,

        NCI_ROUTING_ENTRY_TYPE_TECHNOLOGY,
        3,
        NCI_NFCEE_ID_DH,
        NCI_ROUTING_ENTRY_POWER_ON,
        NCI_RF_TECHNOLOGY_B,
    };

    GDEBUG("%c RF_SET_LISTEN_MODE_ROUTING_CMD (Technology)", DIR_OUT);
    GDEBUG("  NFC-A");
    GDEBUG("  NFC-B");
    g_byte_array_append(cmd, ARRAY_AND_SIZE(static_entries));
    if (sm->options & NCI_OPTION_LISTEN_V) {
        static const guint8 entry[] = {
            NCI_ROUTING_ENTRY_TYPE_TECHNOLOGY,
            3,
            NCI_NFCEE_ID_DH,
            NCI_ROUTING_ENTRY_POWER_ON,
            NCI_RF_TECHNOLOGY_V,
        };

        GDEBUG("  NFC-V");
        cmd->data[1] += 1; /* Number of entries */
        g_byte_array_append(cmd, ARRAY_AND_SIZE(entry));
    }
    if (sm->options & NCI_OPTION_LISTEN_F) {
        static const guint8 entry[] = {
            NCI_ROUTING_ENTRY_TYPE_TECHNOLOGY,
            3,
            NCI_NFCEE_ID_DH,
            NCI_ROUTING_ENTRY_POWER_ON,
            NCI_RF_TECHNOLOGY_F
        };

        GDEBUG("  NFC-F");
        cmd->data[1] += 1; /* Number of entries */
        g_byte_array_append(cmd, ARRAY_AND_SIZE(entry));
    }

    return nci_transition_idle_to_discovery_send_byte_array(self,
        NCI_GID_RF, NCI_OID_RF_SET_LISTEN_MODE_ROUTING, cmd,
        nci_transition_idle_to_discovery_set_tech_routing_rsp);
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
    NciSm* sm = nci_transition_sm(self);

    if (sm) {
        /*
         * Some controllers seem to require RF_SET_LISTEN_MODE_ROUTING,
         * some don't support it at all. Let's give it a try (provided
         * that controller indicated support for protocol based routing
         * in CORE_INIT_RSP) and ignore any errors.
         *
         * Technology based routing works more often, try it first.
         */
        if (sm->nfcc_routing & NCI_NFCC_ROUTING_TECHNOLOGY_BASED) {
            return nci_transition_idle_to_discovery_set_tech_routing(self);
        } else if (sm->nfcc_routing & NCI_NFCC_ROUTING_PROTOCOL_BASED) {
            return nci_transition_idle_to_discovery_set_protocol_routing(self);
        } else {
            return nci_transition_idle_to_discover_map(self);
        }
    }
    return FALSE;
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
