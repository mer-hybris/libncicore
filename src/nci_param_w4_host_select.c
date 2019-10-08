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

#include "nci_param_w4_host_select.h"
#include "nci_param_impl.h"
#include "nci_util.h"

typedef NciParamClass NciParamW4HostSelectClass;
G_DEFINE_TYPE(NciParamW4HostSelect, nci_param_w4_host_select, NCI_TYPE_PARAM)

/*==========================================================================*
 * Interface
 *==========================================================================*/

NciParamW4HostSelect*
nci_param_w4_host_select_new(
    const NciDiscoveryNtf* const* ntf,
    guint count)
{
    NciParamW4HostSelect* self =
        g_object_new(NCI_TYPE_PARAM_W4_HOST_SELECT, NULL);

    self->ntf = nci_discovery_ntf_copy_array(ntf, count);
    self->count = count;
    return self;
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
void
nci_param_w4_host_select_init(
    NciParamW4HostSelect* self)
{
}

static
void
nci_param_w4_host_select_finalize(
    GObject* obj)
{
    NciParamW4HostSelect* self = NCI_PARAM_W4_HOST_SELECT(obj);

    g_free(self->ntf);
    G_OBJECT_CLASS(nci_param_w4_host_select_parent_class)->finalize(obj);
}

static
void
nci_param_w4_host_select_class_init(
    NciParamClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = nci_param_w4_host_select_finalize;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
