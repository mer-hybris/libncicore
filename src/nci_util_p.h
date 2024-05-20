/*
 * Copyright (C) 2019-2024 Slava Monich <slava@monich.com>
 * Copyright (C) 2019-2021 Jolla Ltd.
 * Copyright (C) 2020 Open Mobile Platform LLC.
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

#ifndef NCI_UTIL_PRIVATE_H
#define NCI_UTIL_PRIVATE_H

#include <nci_util.h>
#include "nci_types_p.h"

typedef struct nci_rf_deactivate_ntf {
    NCI_DEACTIVATION_TYPE type;
    NCI_DEACTIVATION_REASON reason;
} NciRfDeactivateNtf;

gboolean
nci_nfcid1_dynamic(
    const NciNfcid1* id)
    NCI_INTERNAL;

gboolean
nci_nfcid1_equal(
    const NciNfcid1* id1,
    const NciNfcid1* id2)
    NCI_INTERNAL;

gboolean
nci_listen_mode(
    NCI_MODE mode)
    NCI_INTERNAL;

guint
nci_parse_config_param_uint(
    guint nparams,
    const GUtilData* params,
    guint8 id,
    guint* value)
    NCI_INTERNAL;

gboolean
nci_parse_config_param_nfcid1(
    guint nparams,
    const GUtilData* params,
    guint8 id,
    NciNfcid1* value)
    NCI_INTERNAL;

const NciModeParam*
nci_parse_mode_param(
    NciModeParam* param,
    NCI_MODE mode,
    const guint8* bytes,
    guint len)
    NCI_INTERNAL;

gboolean
nci_parse_discover_ntf(
    NciDiscoveryNtf* ntf,
    NciModeParam* param,
    const guint8* bytes,
    guint len)
    NCI_INTERNAL;

gboolean
nci_parse_intf_activated_ntf(
    NciIntfActivationNtf* ntf,
    NciModeParam* mode_param,
    NciActivationParam* activation_param,
    const guint8* pkt,
    guint len)
    NCI_INTERNAL;

gboolean
nci_parse_rf_deactivate_ntf(
    NciRfDeactivateNtf* ntf,
    const GUtilData* pkt)
    NCI_INTERNAL;

NciDiscoveryNtf*
nci_discovery_ntf_copy_array(
    const NciDiscoveryNtf* const* ntfs,
    guint count)
    NCI_INTERNAL;

NciDiscoveryNtf*
nci_discovery_ntf_copy(
    const NciDiscoveryNtf* ntf)
    NCI_INTERNAL;

#endif /* NCI_UTIL_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
