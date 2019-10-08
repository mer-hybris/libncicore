/*
 * Copyright (C) 2018-2019 Jolla Ltd.
 * Copyright (C) 2018-2019 Slava Monich <slava.monich@jolla.com>
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

#ifndef NCI_CORE_H
#define NCI_CORE_H

#include <nci_types.h>

G_BEGIN_DECLS

/*
 * If current_state != next_state, the state machine is transitioning
 * from one state to another. That may take a while.
 */

typedef struct nci_core {
    NCI_STATE current_state;
    NCI_STATE next_state;
    guint cmd_timeout;
} NciCore;

typedef
void
(*NciCoreFunc)(
    NciCore* nci,
    void* user_data);

typedef
void
(*NciCoreSendFunc)(
    NciCore* nci,
    gboolean success,
    void* user_data);

typedef
void
(*NciCoreDataPacketFunc)(
    NciCore* nci,
    guint8 cid,
    const void* payload,
    guint len,
    void* user_data);

typedef
void
(*NciCoreIntfActivationFunc)(
    NciCore* nci,
    const NciIntfActivationNtf* ntf,
    void* user_data);

NciCore*
nci_core_new(
    NciHalIo* io);

void
nci_core_free(
    NciCore* nci);

void
nci_core_restart(
    NciCore* nci);

void
nci_core_set_state(
    NciCore* nci,
    NCI_STATE state);

guint
nci_core_send_data_msg(
    NciCore* nci,
    guint8 cid,
    GBytes* payload,
    NciCoreSendFunc complete,
    GDestroyNotify destroy,
    void* user_data);

void
nci_core_cancel(
    NciCore* nci,
    guint id);

gulong
nci_core_add_current_state_changed_handler(
    NciCore* nci,
    NciCoreFunc func,
    void* user_data);

gulong
nci_core_add_next_state_changed_handler(
    NciCore* nci,
    NciCoreFunc func,
    void* user_data);

gulong
nci_core_add_intf_activated_handler(
    NciCore* nci,
    NciCoreIntfActivationFunc func,
    void* user_data);

gulong
nci_core_add_data_packet_handler(
    NciCore* nci,
    NciCoreDataPacketFunc func,
    void* user_data);

void
nci_core_remove_handler(
    NciCore* nci,
    gulong id);

void
nci_core_remove_handlers(
    NciCore* nci,
    gulong* ids,
    guint count);

#define nci_core_remove_all_handlers(core,ids) \
    nci_core_remove_handlers(core, ids, G_N_ELEMENTS(ids))

G_END_DECLS

#endif /* NCI_CORE_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
