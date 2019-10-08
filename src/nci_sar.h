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

#ifndef NCI_SAR_H
#define NCI_SAR_H

#include "nci_types_p.h"

/* SAR (Segmentation and Reassembly) */

typedef struct nci_sar_client NciSarClient;

typedef struct nci_sar_client_functions {
    void (*error)(NciSarClient* client);
    void (*handle_response)(NciSarClient* client, guint8 gid, guint8 oid,
        const void* payload, guint payload_len);
    void (*handle_notification)(NciSarClient* client, guint8 gid, guint8 oid,
        const void* payload, guint payload_len);
    void (*handle_data_packet)(NciSarClient* client, guint8 cid,
        const void* payload, guint payload_len);
} NciSarClientFunctions;

struct nci_sar_client {
    const NciSarClientFunctions* fn;
};

typedef
void
(*NciSarCompletionFunc)(
    NciSarClient* client,
    gboolean success,
    gpointer user_data);

NciSar*
nci_sar_new(
    NciHalIo* io,
    NciSarClient* client);

void
nci_sar_free(
    NciSar* sar);

gboolean
nci_sar_start(
    NciSar* sar);

void
nci_sar_reset(
    NciSar* sar);

void
nci_sar_set_max_logical_connections(
    NciSar* sar,
    guint8 max);

void
nci_sar_set_max_control_packet_size(
    NciSar* sar,
    guint8 max);

void
nci_sar_set_initial_credits(
    NciSar* sar,
    guint8 cid,
    guint8 credits);

void
nci_sar_add_credits(
    NciSar* sar,
    guint8 cid,
    guint8 credits);

guint
nci_sar_send_command(
    NciSar* sar,
    guint8 gid,
    guint8 oid,
    GBytes* payload,
    NciSarCompletionFunc complete,
    GDestroyNotify destroy,
    gpointer user_data);

guint
nci_sar_send_data_packet(
    NciSar* sar,
    guint8 cid,
    GBytes* payload,
    NciSarCompletionFunc complete,
    GDestroyNotify destroy,
    gpointer user_data);

void
nci_sar_cancel(
    NciSar* sar,
    guint id);

#endif /* NFC_SAR_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
