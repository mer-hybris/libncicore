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

#ifndef NCI_STATE_IMPL_H
#define NCI_STATE_IMPL_H

#include "nci_state.h"
#include "nci_types_p.h"

/* Internal API for use by NciState implemenations */

typedef struct nci_state_class {
    GObjectClass parent;
    void (*enter)(NciState* state, NciParam* param);
    void (*reenter)(NciState* state, NciParam* param);
    void (*leave)(NciState* state);
    void (*handle_ntf)(NciState* state, guint8 gid, guint8 oid,
        const GUtilData* payload);
} NciStateClass;

#define NCI_STATE_CLASS(klass) G_TYPE_CHECK_CLASS_CAST((klass), \
        NCI_TYPE_STATE, NciStateClass)

void
nci_state_init_base(
    NciState* state,
    NciSm* sm,
    NCI_STATE id,
    const char* name);

gboolean
nci_state_send_command(
    NciState* state,
    guint8 gid,
    guint8 oid,
    GBytes* payload,
    NciSmResponseFunc resp,
    gpointer user_data);

void
nci_state_error(
    NciState* state);

NciSm*
nci_state_sm(
    NciState* state);

#endif /* NCI_STATE_IMPL_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
