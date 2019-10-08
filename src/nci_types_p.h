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

#ifndef NCI_TYPES_PRIVATE_H
#define NCI_TYPES_PRIVATE_H

#include <nci_types.h>

typedef struct nci_param NciParam;
typedef struct nci_sar NciSar;
typedef struct nci_sm NciSm;
typedef struct nci_transition NciTransition;

typedef enum nci_request_status {
    NCI_REQUEST_SUCCESS,
    NCI_REQUEST_TIMEOUT,
    NCI_REQUEST_CANCELLED
} NCI_REQUEST_STATUS;

typedef
void
(*NciSmResponseFunc)(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    gpointer user_data);

/* Stall modes */
typedef enum nci_stall {
    NCI_STALL_STOP,
    NCI_STALL_ERROR
} NCI_STALL;

/* Debug log */
#define DIR_IN  '>'
#define DIR_OUT '<'

/* Message Type (MT) */
#define NCI_MT_MASK     (0xe0)
#define NCI_MT_DATA_PKT (0x00)
#define NCI_MT_CMD_PKT  (0x20)
#define NCI_MT_RSP_PKT  (0x40)
#define NCI_MT_NTF_PKT  (0x60)

/* Packet Boundary Flag (PBF) */
#define NCI_PBF         (0x10)

#endif /* NCI_TYPES_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
