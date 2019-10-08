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

#include "nci_param_w4_all_discoveries.h"
#include "nci_param_impl.h"
#include "nci_util.h"

typedef NciParamClass NciParamW4AllDiscoveriesClass;
G_DEFINE_TYPE(NciParamW4AllDiscoveries, nci_param_w4_all_discoveries,
    NCI_TYPE_PARAM)

/*==========================================================================*
 * Interface
 *==========================================================================*/

NciParamW4AllDiscoveries*
nci_param_w4_all_discoveries_new(
    const NciDiscoveryNtf* ntf)
{
    if (ntf) {
        NciParamW4AllDiscoveries* self =
            g_object_new(NCI_TYPE_PARAM_W4_ALL_DISCOVERIES, NULL);

        self->ntf = nci_discovery_ntf_copy(ntf);
        return self;
    }
    return NULL;
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
void
nci_param_w4_all_discoveries_init(
    NciParamW4AllDiscoveries* self)
{
}

static
void
nci_param_w4_all_discoveries_finalize(
    GObject* obj)
{
    NciParamW4AllDiscoveries* self = NCI_PARAM_W4_ALL_DISCOVERIES(obj);

    g_free(self->ntf);
    G_OBJECT_CLASS(nci_param_w4_all_discoveries_parent_class)->finalize(obj);
}

static
void
nci_param_w4_all_discoveries_class_init(
    NciParamClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = nci_param_w4_all_discoveries_finalize;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
