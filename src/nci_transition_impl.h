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

#ifndef NCI_TRANSITION_IMPL_H
#define NCI_TRANSITION_IMPL_H

#include "nci_transition.h"

/* Internal API for use by NciTransition implemenations */

typedef struct nci_transition_class {
    GObjectClass parent;
    gboolean (*start)(NciTransition* self);
    void (*finished)(NciTransition* self);
    void (*handle_ntf)(NciTransition* self, guint8 gid, guint8 oid,
        const GUtilData* payload);
} NciTransitionClass;

#define NCI_TRANSITION_CLASS(klass) G_TYPE_CHECK_CLASS_CAST((klass), \
        NCI_TYPE_TRANSITION, NciTransitionClass)

typedef
void
(*NciTransitionResponseFunc)(
    NCI_REQUEST_STATUS status,
    const GUtilData* payload,
    NciTransition* transition);

void
nci_transition_init_base(
    NciTransition* self,
    NciSm* sm,
    NciState* dest);

NciSm*
nci_transition_sm(
    NciTransition* self);

void
nci_transition_finish(
    NciTransition* self,
    void* param);

void
nci_transition_stall(
    NciTransition* self,
    NCI_STALL stall);

gboolean
nci_transition_active(
    NciTransition* self);

gboolean
nci_transition_send_command(
    NciTransition* self,
    guint8 gid,
    guint8 oid,
    GBytes* payload,
    NciTransitionResponseFunc resp);

gboolean
nci_transition_send_command_static(
    NciTransition* self,
    guint8 gid,
    guint8 oid,
    const void* payload,
    gsize payload_len,
    NciTransitionResponseFunc resp);

gboolean
nci_transition_deactivate_to_idle(
    NciTransition* self,
    NciTransitionResponseFunc resp);

gboolean
nci_transition_deactivate_to_discovery(
    NciTransition* self,
    NciTransitionResponseFunc resp);

#endif /* NCI_TRANSITION_IMPL_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
