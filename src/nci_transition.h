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

#ifndef NCI_TRANSITION_H
#define NCI_TRANSITION_H

#include "nci_types_p.h"

#include <glib-object.h>

/* State transition */

typedef struct nci_transition_priv NciTransitionPriv;

struct nci_transition {
    GObject object;
    NciTransitionPriv* priv;
    NciState* dest;
};

GType nci_transition_get_type(void);
#define NCI_TYPE_TRANSITION (nci_transition_get_type())
#define NCI_TRANSITION(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
        NCI_TYPE_TRANSITION, NciTransition))

typedef
void
(*NciTransitionFunc)(
    NciTransition* transition,
    void* user_data);

NciTransition*
nci_transition_ref(
    NciTransition* transition);

void
nci_transition_unref(
    NciTransition* transition);

gboolean
nci_transition_start(
    NciTransition* self);

void
nci_transition_finished(
    NciTransition* self);

void
nci_transition_handle_ntf(
    NciTransition* transition,
    guint8 gid,
    guint8 oid,
    const GUtilData* payload);

/* Specific transitions */

NciTransition* 
nci_transition_reset_new(
    NciSm* sm);

NciTransition* 
nci_transition_idle_to_discovery_new(
    NciSm* sm);

NciTransition* 
nci_transition_deactivate_to_idle_new(
    NciSm* sm);

NciTransition* 
nci_transition_poll_active_to_discovery_new(
    NciSm* sm);

NciTransition* 
nci_transition_poll_active_to_idle_new(
    NciSm* sm);

#endif /* NCI_TRANSITION_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
