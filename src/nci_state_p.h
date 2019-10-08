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

#ifndef NCI_STATE_PRIVATE_H
#define NCI_STATE_PRIVATE_H

#include "nci_types_p.h"

#include "nci_state.h"

NciTransition*
nci_state_get_transition(
    NciState* state,
    NCI_STATE dest);

void
nci_state_add_transition(
    NciState* state,
    NciTransition* transition);

void
nci_state_enter(
    NciState* state,
    void* param);

void
nci_state_reenter(
    NciState* state,
    void* param);

void
nci_state_leave(
    NciState* state);

void
nci_state_handle_ntf(
    NciState* state,
    guint8 gid,
    guint8 oid,
    const GUtilData* payload);

/* Specific states */

NciState* /* NCI_STATE_INIT */
nci_state_init_new(
    NciSm* sm);

NciState* /* NCI_STATE_ERROR */
nci_state_error_new(
    NciSm* sm);

NciState* /* NCI_STATE_STOP */
nci_state_stop_new(
    NciSm* sm);

NciState* /* NCI_RFST_IDLE */
nci_state_idle_new(
    NciSm* sm);

NciState* /* NCI_RFST_DISCOVERY */
nci_state_discovery_new(
    NciSm* sm);

NciState* /* NCI_RFST_POLL_ACTIVE */
nci_state_poll_active_new(
    NciSm* sm);

NciState* /* NCI_RFST_W4_ALL_DISCOVERIES */
nci_state_w4_all_discoveries_new(
    NciSm* sm);

NciState* /* NCI_RFST_W4_HOST_SELECT */
nci_state_w4_host_select_new(
    NciSm* sm);

#endif /* NCI_STATE_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
