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

#include "nci_sar.h"
#include "nci_hal.h"
#include "nci_log.h"

#include <gutil_macros.h>

/* SAR (Segmentation and Reassembly) */

#define SAR_DEFAULT_MAX_LOGICAL_CONNECTIONS (1)
#define SAR_DEFAULT_CONTROL_MTU (0x20)
#define SAR_DEFAULT_DATA_MTU (0xff)
#define SAR_MIN_MTU (0x04)
#define SAR_UNLIMITED_CREDITS (0xff)

#define NCI_HDR_SIZE (3)

typedef struct nci_sar_packet_out NciSarPacketOut;

struct nci_sar_packet_out {
    NciSarPacketOut* next;
    guint8 hdr[NCI_HDR_SIZE];
    GBytes* payload;
    guint payload_pos;
    NciSarCompletionFunc complete;
    GDestroyNotify destroy;
    gpointer user_data;
    guint id;
};

typedef struct nci_sar_packet_out_queue {
    NciSarPacketOut* first;
    NciSarPacketOut* last;
} NciSarPacketOutQueue;

typedef struct nci_sar_logical_connection {
    guint8 credits;
    GByteArray* in;
    NciSarPacketOutQueue out;
} NciSarLogicalConnection;

struct nci_sar {
    NciHalIo* io;
    NciSarClient* client;
    NciHalClient hal_client;
    gboolean started;
    guint8 max_logical_conns;
    guint8 control_mtu;
    guint8 data_mtu;
    guint last_packet_id;
    guint start_write_id;
    gboolean write_pending;
    NciSarPacketOut* writing;
    NciSarPacketOutQueue cmd;
    NciSarLogicalConnection* conn;
    GByteArray* control_in;
    GByteArray* read_buf;
};

/* Control packets */

#define NCI_CONTROL_GID_MASK    (0x0f)  /* First octet */
#define NCI_CONTROL_OID_MASK    (0x3f)  /* Second octet */

/* Data packets */
#define NCI_DATA_CID_MASK       (0x0f)

static
void
nci_sar_schedule_write(
    NciSar* self);

static
void
nci_sar_attempt_write(
    NciSar* self);

static
NciSar*
nci_sar_from_hal_client(
    NciHalClient* hal_client)
{
    return G_CAST(hal_client, NciSar, hal_client);
}

static
void
nci_sar_packet_out_free(
    NciSarPacketOut* out)
{
    /* Caller makes sure that argument is not NULL */
    if (out->payload) {
        g_bytes_unref(out->payload);
    }
    if (out->destroy) {
        out->destroy(out->user_data);
    }
    g_slice_free(NciSarPacketOut, out);
}

static
void
nci_sar_write_completed(
    NciHalClient* client,
    gboolean ok)
{
    NciSar* self = nci_sar_from_hal_client(client);
    NciSarPacketOut* out = self->writing;

    GASSERT(out);
    GASSERT(self->write_pending);
    self->write_pending = FALSE;
    if (ok) {
        gsize payload_len = out->payload ? g_bytes_get_size(out->payload) : 0;

        GASSERT(payload_len >= out->payload_pos);
        if (payload_len == out->payload_pos) {
            /* Done with this packet */
            self->writing = NULL;
            if (out->complete) {
                out->complete(self->client, TRUE, out->user_data);
            }
            nci_sar_packet_out_free(out);
        }

        /* And try to write the next segment or packet */
        nci_sar_attempt_write(self);
    } else {
        NciSarClient* client = self->client;

        /* Drop this packet and indicate an error */
        self->writing = NULL;
        if (out->complete) {
            out->complete(self->client, FALSE, out->user_data);
        }
        nci_sar_packet_out_free(out);
        client->fn->error(client);
    }
}

static
NciSarPacketOutQueue*
nci_sar_write_queue(
    NciSar* self,
    gboolean eat_credit)
{
    if (self->cmd.first) {
        return &self->cmd;
    } else {
        guint i;

        for (i = 0; i < self->max_logical_conns; i++) {
            NciSarLogicalConnection* conn = self->conn + i;

            if (conn->out.first && conn->credits) {
                if (eat_credit) {
                    conn->credits--;
                }
                return &conn->out;
            }
        }
    }
    return NULL;
}

static
gboolean
nci_sar_can_write(
    NciSar* self)
{
    return !self->writing && nci_sar_write_queue(self, FALSE);
}

static
void
nci_sar_attempt_write(
    NciSar* self)
{
    if (!self->write_pending) {
        if (!self->writing) {
            NciSarPacketOutQueue* queue = nci_sar_write_queue(self, TRUE);

            if (queue) {
                self->writing = queue->first;
                queue->first = self->writing->next;
                self->writing->next = NULL;
                if (!queue->first) {
                    queue->last = NULL;
                }
            }
        }
        if (self->writing) {
            NciHalIo* io = self->io;
            NciSarPacketOut* out = self->writing;
            guint total_len = NCI_HDR_SIZE; /* +1 for payload length */
            const guint8* payload = NULL;
            gsize remaining_payload_len = 0;
            GUtilData chunks[2];
            int nchunks = 1;
            gboolean write_submitted = FALSE;
            const guint mtu = ((out->hdr[0] & NCI_MT_MASK) ==
                NCI_MT_CMD_PKT) ? self->control_mtu : self->data_mtu;

            if (out->payload) {
                gsize payload_len = 0;

                payload = g_bytes_get_data(out->payload, &payload_len);
                GASSERT(payload_len >= out->payload_pos);
                remaining_payload_len = payload_len - out->payload_pos;
                total_len += remaining_payload_len;
            }

            chunks[0].bytes = out->hdr;
            chunks[0].size = NCI_HDR_SIZE;
            if (total_len <= mtu) {
                /* We can send the whole thing */
                out->hdr[0] &= ~NCI_PBF;
                out->hdr[2] = (guint8)remaining_payload_len;
                if (remaining_payload_len) {
                    chunks[nchunks].bytes = payload + out->payload_pos;
                    chunks[nchunks].size = remaining_payload_len;
                    out->payload_pos += remaining_payload_len;
                    nchunks++;
                }
            } else {
                /* Send a fragment */
                out->hdr[0] |= NCI_PBF;
                out->hdr[2] = mtu;
                chunks[nchunks].bytes = payload;
                chunks[nchunks].size = mtu - chunks[0].size;
                out->payload_pos += chunks[nchunks].size;
                nchunks++;
            }

            /* Start HAL on demand */
            if (!self->started) {
                self->started = io->fn->start(io, &self->hal_client);
            }

            /* Submit write request to the HAL */
            if (self->started) {
                self->write_pending = TRUE;
                if (io->fn->write(io, chunks, nchunks,
                    nci_sar_write_completed)) {
                    write_submitted = TRUE;
                } else {
                    self->write_pending = FALSE;
                }
            }

            /* Bail out if something went wrong */
            if (!write_submitted) {
                NciSarClient* client = self->client;

                /* Drop this packet and indicate an error */
                self->writing = NULL;
                if (out->complete) {
                    out->complete(self->client, FALSE, out->user_data);
                }
                nci_sar_packet_out_free(out);
                client->fn->error(client);

                /* Try the next one even though it will probably fail too */
                nci_sar_schedule_write(self);
            }
        }
    }
}

static
gboolean
nci_sar_start_write(
    gpointer user_data)
{
    NciSar* self = user_data;

    self->start_write_id = 0;
    nci_sar_attempt_write(self);
    return G_SOURCE_REMOVE;
}

static
void
nci_sar_schedule_write(
    NciSar* self)
{
    if (!self->start_write_id && nci_sar_can_write(self)) {
        self->start_write_id = g_idle_add(nci_sar_start_write, self);
    }
}

static
guint
nci_sar_send(
    NciSar* self,
    NciSarPacketOutQueue* queue,
    const guint8* hdr,
    GBytes* payload,
    NciSarCompletionFunc complete,
    GDestroyNotify destroy,
    gpointer user_data)
{
    guint id = 0;
    NciSarPacketOut* out = g_slice_new0(NciSarPacketOut);

    /* Generate id */
    id = (++self->last_packet_id);
    if (!id) id = (++self->last_packet_id);

    /* Fill in the packet structure */
    out->id = id;
    out->complete = complete;
    out->destroy = destroy;
    out->user_data = user_data;
    memcpy(out->hdr, hdr, NCI_HDR_SIZE - 1); /* Ignore the length */
    if (payload) {
        out->payload = g_bytes_ref(payload);
    }

    /* Queue the packet */
    if (queue->last) {
        GASSERT(queue->first);
        queue->last->next = out;
        queue->last = out;
    } else {
        GASSERT(!queue->first);
        queue->first = queue->last = out;
    }

    /* Schedule write */
    nci_sar_schedule_write(self);
    return id;
}

static
void
nci_sar_hal_handle_control_packet(
    NciSar* self,
    guint8 mt,
    guint8 gid,
    guint8 oid,
    const guint8* payload,
    guint payload_len)
{
    NciSarClient* client = self->client;

    if (mt == NCI_MT_RSP_PKT) {
        client->fn->handle_response(client, gid, oid, payload, payload_len);
    } else {
        GASSERT(mt == NCI_MT_NTF_PKT);
        client->fn->handle_notification(client, gid, oid, payload, payload_len);
    }
}

static
void
nci_sar_hal_handle_data_packet(
    NciSar* self,
    guint cid,
    const guint8* payload,
    guint payload_len)
{
    NciSarClient* client = self->client;

    client->fn->handle_data_packet(client, cid, payload, payload_len);
}

static
void
nci_sar_hal_handle_control_segment(
    NciSar* self,
    const guint8* packet,
    guint len)
{
    const guint8 hdr = packet[0];
    const guint8 mt = (hdr & NCI_MT_MASK);
    const guint8 gid = (hdr & NCI_CONTROL_GID_MASK);
    const guint8 oid = (packet[1] & NCI_CONTROL_OID_MASK);
    const guint8 payload_len = packet[2];
    const guint8* payload = packet + NCI_HDR_SIZE;
    NciSarClient* client = self->client;
    GByteArray* in = self->control_in;

    /* For each segment of a Control Message, the header of the
     * Control Packet SHALL contain the same MT, GID and OID values. */
    if (in && in->len > 0) {
        if (mt == (in->data[0] & NCI_MT_MASK) &&
            gid == (in->data[0] & NCI_CONTROL_GID_MASK) &&
            oid == (in->data[1] & NCI_CONTROL_OID_MASK)) {
            /* Only append the payload */
            g_byte_array_append(in, payload, payload_len);
            if (!(hdr & NCI_PBF)) {
                /* This was the last segment. Since we are about to
                 * invoke external code, zero self->control_in to
                 * make sure that data doesn't get modified. */
                self->control_in = NULL;
                nci_sar_hal_handle_control_packet(self, mt, gid, oid,
                    in->data + NCI_HDR_SIZE, in->len - NCI_HDR_SIZE);
                g_byte_array_set_size(in, 0);
                if (!self->control_in) {
                    /* Restore the pointer */
                    self->control_in = in;
                } else {
                    g_byte_array_free(in, TRUE);
                }
            }
        } else {
            GDEBUG("MT/GID/OID mismatch for segmented control packet");
            client->fn->error(client);
        }
    } else if (hdr & NCI_PBF) {
        /* First segment of a segmented packet */
        if (!in) {
            in = self->control_in = g_byte_array_new();
        }
        g_byte_array_append(in, packet, payload_len + NCI_HDR_SIZE);
    } else {
        /* Complete packet */
        nci_sar_hal_handle_control_packet(self, mt, gid, oid, payload,
            payload_len);
    }
}

static
void
nci_sar_hal_handle_data_segment(
    NciSar* self,
    const guint8* packet,
    guint len)
{
    const guint8 hdr = packet[0];
    const guint8 cid = (hdr & NCI_DATA_CID_MASK);
    const guint8 payload_len = packet[2];
    const guint8* payload = packet + NCI_HDR_SIZE;
    NciSarClient* client = self->client;

    if (cid < self->max_logical_conns) {
        NciSarLogicalConnection* conn = self->conn + cid;
        GByteArray* in = conn->in;

        if (in && in->len > 0) {
            /* Only append the payload */
            g_byte_array_append(in, payload, payload_len);
            if (!(hdr & NCI_PBF)) {
                /* This was the last segment. Since we are about to
                 * invoke external code, zero data_in[cid] to ensure
                 * that data doesn't get modified. */
                conn->in = NULL;
                nci_sar_hal_handle_data_packet(self, cid,
                    in->data + NCI_HDR_SIZE, in->len - NCI_HDR_SIZE);
                g_byte_array_set_size(in, 0);
                if (!conn->in) {
                    /* Restore the pointer */
                    conn->in = in;
                } else {
                    g_byte_array_free(in, TRUE);
                }
            }
        } else if (hdr & NCI_PBF) {
            /* First segment of a segmented packet */
            if (!in) {
                in = conn->in = g_byte_array_new();
            }
            g_byte_array_append(in, packet, payload_len + NCI_HDR_SIZE);
        } else {
            /* Complete packet */
            nci_sar_hal_handle_data_packet(self, cid, payload, payload_len);
        }
    } else {
        GDEBUG("Invalid logical connection 0x%02x", cid);
        client->fn->error(client);
    }
}

static
void
nci_sar_hal_handle_segment(
    NciSar* self,
    const guint8* packet,
    guint len)
{
    const guint8 mt = packet[0] & NCI_MT_MASK;
    NciSarClient* client = self->client;

    switch (mt) {
    case NCI_MT_DATA_PKT:
        nci_sar_hal_handle_data_segment(self, packet, len);
        break;
    case NCI_MT_RSP_PKT:
    case NCI_MT_NTF_PKT:
        nci_sar_hal_handle_control_segment(self, packet, len);
        break;
    case NCI_MT_CMD_PKT:
        /* We are not supposed to receive a command from NFCC */
    default:
        GDEBUG("Unsupported message type 0x%02x", mt);
        client->fn->error(client);
        break;
    }
}

/*==========================================================================*
 * HAL client
 *==========================================================================*/

static
void
nci_sar_hal_client_error(
    NciHalClient* hal_client)
{
    NciSar* self = nci_sar_from_hal_client(hal_client);
    NciSarClient* client = self->client;

    /* Forward it to our client */
    client->fn->error(client);
}

static
void
nci_sar_hal_client_read(
    NciHalClient* hal_client,
    const void* data,
    guint len)
{
    NciSar* self = nci_sar_from_hal_client(hal_client);
    const guint8* bytes = data;

    if (!self->read_buf || !self->read_buf->len) {
        /* Optimal (and usual) case - full packets are coming in */
        while (len > 2 && len >= (bytes[2] + NCI_HDR_SIZE)) {
            const guint packet_len = bytes[2] + NCI_HDR_SIZE;

            nci_sar_hal_handle_segment(self, bytes, packet_len);
            bytes += packet_len;
            len -= packet_len;
        }
    }

    if (len > 0) {
        GByteArray* buf = self->read_buf;

        if (!buf) {
            buf = self->read_buf = g_byte_array_new();
        }

        /* This is something non-trivial (partial or fragmented packet) */
        g_byte_array_append(buf, bytes, len);
        while (buf->len > 2 && buf->len >= (buf->data[2] + NCI_HDR_SIZE)) {
            const guint packet_len = buf->data[2] + NCI_HDR_SIZE;

            nci_sar_hal_handle_segment(self, buf->data, packet_len);
            g_byte_array_remove_range(buf, 0, packet_len);
        }
    }
}

static
void
nci_sar_clear_queue(
    NciSarPacketOutQueue* queue)
{
    while (queue->first) {
        NciSarPacketOut* out = queue->first;

        queue->first = out->next;
        nci_sar_packet_out_free(out);
    }
    queue->last = NULL;
}

static
gboolean
nci_sar_cancel_queue(
    NciSar* self,
    NciSarPacketOutQueue* queue,
    guint id)
{
    if (queue->first) {
        NciSarPacketOut* out = queue->first;

        if (out->id == id) {
            if (!(queue->first = out->next)) {
                queue->last = NULL;
            }
            nci_sar_packet_out_free(out);
            return TRUE;
        } else {
            NciSarPacketOut* prev = out;

            out = out->next;
            while (out) {
                if (out->id == id) {
                    prev->next = out->next;
                    if (!prev->next) {
                        queue->last = prev;
                    }
                    nci_sar_packet_out_free(out);
                    return TRUE;
                }
                prev = out;
                out = out->next;
            }
        }
    }
    return FALSE;
}

/*==========================================================================*
 * Interface
 *==========================================================================*/

NciSar*
nci_sar_new(
    NciHalIo* io,
    NciSarClient* client)
{
    NciSar* self = g_slice_new0(NciSar);
    static const NciHalClientFunctions hal_functions = {
        .error = nci_sar_hal_client_error,
        .read = nci_sar_hal_client_read
    };

    self->hal_client.fn = &hal_functions;
    self->client = client;
    self->io = io;
    self->max_logical_conns = SAR_DEFAULT_MAX_LOGICAL_CONNECTIONS;
    self->control_mtu = SAR_DEFAULT_CONTROL_MTU;
    self->data_mtu = SAR_DEFAULT_DATA_MTU;
    self->conn = g_new0(NciSarLogicalConnection, self->max_logical_conns);
    return self;
}

void
nci_sar_free(
    NciSar* self)
{
    if (G_LIKELY(self)) {
        guint i;

        nci_sar_reset(self);

        /* Free things that nci_sar_reset() didn't deallocate */
        for (i = 0; i < self->max_logical_conns; i++) {
            NciSarLogicalConnection* conn = self->conn + i;

            if (conn->in) {
                g_byte_array_free(conn->in, TRUE);
            }
        }
        if (self->control_in) {
            g_byte_array_free(self->control_in, TRUE);
        }
        if (self->read_buf) {
            g_byte_array_free(self->read_buf, TRUE);
        }
        g_free(self->conn);
        g_slice_free(NciSar, self);
    }
}

gboolean
nci_sar_start(
    NciSar* self)
{
    if (G_LIKELY(self)) {
        NciHalIo* io = self->io;

        if (!self->started) {
            self->started = io->fn->start(io, &self->hal_client);
        }
        return self->started;
    }
    return FALSE;
}

void
nci_sar_reset(
    NciSar* self)
{
    if (G_LIKELY(self)) {
        guint i;

        if (self->started) {
            NciHalIo* io = self->io;

            self->started = FALSE;
            io->fn->stop(io);
        }

        for (i = 0; i < self->max_logical_conns; i++) {
            NciSarLogicalConnection* conn = self->conn + i;

            nci_sar_clear_queue(&conn->out);
            conn->credits = 0;
            if (conn->in) {
                g_byte_array_set_size(conn->in, 0);
            }
        }

        if (self->control_in) {
            g_byte_array_set_size(self->control_in, 0);
        }

        if (self->writing) {
            /* Stop canceled it already */
            nci_sar_packet_out_free(self->writing);
            self->writing = NULL;
        }

        nci_sar_clear_queue(&self->cmd);
        if (self->start_write_id) {
            g_source_remove(self->start_write_id);
            self->start_write_id = 0;
        }
    }
}

void
nci_sar_set_max_logical_connections(
    NciSar* self,
    guint8 max)
{
    if (G_LIKELY(self)) {
        guint i;

        if (!max) {
            max = SAR_DEFAULT_MAX_LOGICAL_CONNECTIONS;
        }
        if (max > self->max_logical_conns) {
            self->conn = g_realloc(self->conn, sizeof(self->conn[0]) * max);
            memset(self->conn + self->max_logical_conns, 0,
                sizeof(self->conn[0]) * (max - self->max_logical_conns));
            self->max_logical_conns = max;
        } else if (max < self->max_logical_conns) {
            for (i = max; i < self->max_logical_conns; i++) {
                NciSarLogicalConnection* conn = self->conn + i;

                nci_sar_clear_queue(&conn->out);
                if (conn->in) {
                    g_byte_array_free(conn->in, TRUE);
                }
            }
            self->conn = g_realloc(self->conn, sizeof(self->conn[0]) * max);
            self->max_logical_conns = max;
        }
    }
}

void
nci_sar_set_max_control_packet_size(
    NciSar* self,
    guint8 max)
{
    if (G_LIKELY(self)) {
        if (max < SAR_MIN_MTU) {
            self->control_mtu = max ? SAR_MIN_MTU : SAR_DEFAULT_CONTROL_MTU;
        } else  {
            self->control_mtu = max;
        }
    }
}

void
nci_sar_set_initial_credits(
    NciSar* self,
    guint8 cid,
    guint8 credits)
{
    if (G_LIKELY(self) && cid < self->max_logical_conns) {
        /* The queue should be empty at this point */
        GASSERT(!self->conn[cid].out.first);
        self->conn[cid].credits = credits;
    }
}

void
nci_sar_add_credits(
    NciSar* self,
    guint8 cid,
    guint8 credits)
{
    if (G_LIKELY(self) && cid < self->max_logical_conns) {
        NciSarLogicalConnection* conn = self->conn + cid;

        if (credits > (0xff - conn->credits)) {
            GWARN("Credits overflow");
            conn->credits = SAR_UNLIMITED_CREDITS;
        } else {
            conn->credits += credits;
        }
        if (conn->out.first) {
            nci_sar_schedule_write(self);
        }
    }
}

guint
nci_sar_send_command(
    NciSar* self,
    guint8 gid,
    guint8 oid,
    GBytes* payload,
    NciSarCompletionFunc complete,
    GDestroyNotify destroy,
    gpointer user_data)
{
    GASSERT(!(gid & ~NCI_CONTROL_GID_MASK));
    GASSERT(!(oid & ~NCI_CONTROL_OID_MASK));
    if (G_LIKELY(self)) {
        guint8 hdr[NCI_HDR_SIZE];

        hdr[0] = NCI_MT_CMD_PKT | (gid & NCI_CONTROL_GID_MASK);
        hdr[1] = oid & NCI_CONTROL_OID_MASK;
        return nci_sar_send(self, &self->cmd, hdr, payload, complete,
            destroy, user_data);
    }
    return 0;
}

guint
nci_sar_send_data_packet(
    NciSar* self,
    guint8 cid,
    GBytes* payload,
    NciSarCompletionFunc complete,
    GDestroyNotify destroy,
    gpointer user_data)
{
    GASSERT(!(cid & ~NCI_DATA_CID_MASK));
    if (G_LIKELY(self) && cid < self->max_logical_conns) {
        NciSarLogicalConnection* conn = self->conn + cid;
        guint8 hdr[NCI_HDR_SIZE];

        hdr[0] = cid & NCI_DATA_CID_MASK;
        hdr[1] = 0;
        return nci_sar_send(self, &conn->out, hdr, payload, complete,
            destroy, user_data);
    }
    return 0;
}

void
nci_sar_cancel(
    NciSar* self,
    guint id)
{
    if (G_LIKELY(self) && G_LIKELY(id)) {
        if (self->writing && self->writing->id == id) {
            /* We can't really cancel the packet once we started writing it.
             * Just clear the completion callback. */
            self->writing->complete = NULL;
            return;
        } else if (!nci_sar_cancel_queue(self, &self->cmd, id)) {
            guint i;

            for (i = 0; i < self->max_logical_conns; i++) {
                NciSarLogicalConnection* conn = self->conn + i;

                if (nci_sar_cancel_queue(self, &conn->out, id)) {
                    return;
                }
            }
        }
        GWARN("Invalid packet id %u", id);
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
