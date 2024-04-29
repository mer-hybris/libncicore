/*
 * Copyright (C) 2019-2024 Slava Monich <slava@monich.com>
 * Copyright (C) 2019-2021 Jolla Ltd.
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

#ifndef NCI_STATE_MACHINE_H
#define NCI_STATE_MACHINE_H

#include "nci_types_p.h"

/* RF Communication State Machine */
typedef struct nci_sm_io NciSmIo;

struct nci_sm_io {
    NciSar* sar;
    gboolean (*send)(NciSmIo* io, guint8 gid, guint8 oid,
        GBytes* payload, NciSmResponseFunc resp, gpointer user_data);
    void (*cancel)(NciSmIo* io);
};

struct nci_sm {
    NciSmIo* io;
    NciState* last_state;
    NciState* next_state;
    GBytes* rf_interfaces;
    guint max_routing_table_size;
    NCI_TECH techs;
    NCI_INTERFACE_VERSION version;
    NCI_NFCC_DISCOVERY nfcc_discovery;
    NCI_NFCC_ROUTING nfcc_routing;
    NCI_NFCC_POWER nfcc_power;
    NCI_OP_MODE op_mode;
    guint8 llc_version;
    guint16 llc_wks;
    NciNfcid1 la_nfcid1; /* NFCID1 in Listen A mode */
};

typedef
void
(*NciSmFunc)(
    NciSm* sm,
    void* user_data);

typedef
void
(*NciSmIntfActivationFunc)(
    NciSm* sm,
    const NciIntfActivationNtf* ntf,
    void* user_data);

/* Universal interface, used all over the place */

NciState*
nci_sm_enter_state(
    NciSm* sm,
    NCI_STATE id,
    NciParam* param)
    NCI_INTERNAL;

void
nci_sm_switch_to(
    NciSm* sm,
    NCI_STATE id)
    NCI_INTERNAL;

void
nci_sm_stall(
    NciSm* sm,
    NCI_STALL type)
    NCI_INTERNAL;

#define nci_sm_error(sm) \
    nci_sm_stall(sm, NCI_STALL_ERROR)

/* Interface for NciCore */

NciSm*
nci_sm_new(
    NciSmIo* io)
    NCI_INTERNAL;

void
nci_sm_free(
    NciSm* sm)
    NCI_INTERNAL;

void
nci_sm_set_op_mode(
    NciSm* sm,
    NCI_OP_MODE op_mode)
    NCI_INTERNAL;

NCI_TECH
nci_sm_set_tech(
    NciSm* sm,
    NCI_TECH tech)
    NCI_INTERNAL;

void
nci_sm_set_la_nfcid1(
    NciSm* sm,
    const NciNfcid1* nfcid1)
    NCI_INTERNAL;

void
nci_sm_handle_ntf(
    NciSm* sm,
    guint8 gid,
    guint8 oid,
    const GUtilData* payload)
    NCI_INTERNAL;

void
nci_sm_add_state(
    NciSm* sm,
    NciState* state)
    NCI_INTERNAL;

void
nci_sm_add_transition(
    NciSm* sm,
    NCI_STATE state,
    NciTransition* transition)
    NCI_INTERNAL;

gulong
nci_sm_add_last_state_handler(
    NciSm* sm,
    NciSmFunc func,
    void* user_data)
    NCI_INTERNAL;

gulong
nci_sm_add_next_state_handler(
    NciSm* sm,
    NciSmFunc func,
    void* user_data)
    NCI_INTERNAL;

gulong
nci_sm_add_intf_activated_handler(
    NciSm* sm,
    NciSmIntfActivationFunc func,
    void* user_data)
    NCI_INTERNAL;

void
nci_sm_remove_handler(
    NciSm* sm,
    gulong id)
    NCI_INTERNAL;

void
nci_sm_remove_handlers(
    NciSm* sm,
    gulong* ids,
    guint count)
    NCI_INTERNAL;

#define nci_sm_remove_all_handlers(sm,ids) \
    nci_sm_remove_handlers(sm, ids, G_N_ELEMENTS(ids))

/* Interface for states and transitions */

void
nci_sm_add_weak_pointer(
    NciSm** ptr)
    NCI_INTERNAL;

void
nci_sm_remove_weak_pointer(
    NciSm** ptr)
    NCI_INTERNAL;

NciState*
nci_sm_get_state(
    NciSm* sm,
    NCI_STATE state)
    NCI_INTERNAL;

NciSar*
nci_sm_sar(
    NciSm* sm)
    NCI_INTERNAL;

gboolean
nci_sm_supports_protocol(
    NciSm* sm,
    NCI_PROTOCOL protocol)
    NCI_INTERNAL;

gboolean
nci_sm_active_transition(
    NciSm* sm,
    NciTransition* transition)
    NCI_INTERNAL;

gboolean
nci_sm_send_command(
    NciSm* sm,
    guint8 gid,
    guint8 oid,
    GBytes* payload,
    NciSmResponseFunc resp,
    gpointer user_data)
    NCI_INTERNAL;

gboolean
nci_sm_send_command_static(
    NciSm* sm,
    guint8 gid,
    guint8 oid,
    const void* payload,
    gsize payload_len,
    NciSmResponseFunc resp,
    gpointer user_data)
    NCI_INTERNAL;

void
nci_sm_intf_activated(
    NciSm* sm,
    const NciIntfActivationNtf* ntf)
    NCI_INTERNAL;

void
nci_sm_handle_conn_credits_ntf(
    NciSm* sm,
    const GUtilData* payload)
    NCI_INTERNAL;

void
nci_sm_handle_rf_deactivate_ntf(
    NciSm* sm,
    const GUtilData* payload)
    NCI_INTERNAL;

/* And this one is currently only for unit tests */

extern const char* nci_sm_config_file NCI_INTERNAL;

#endif /* NCI_STATE_MACHINE_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
