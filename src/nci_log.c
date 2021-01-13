/*
 * Copyright (C) 2021 Jolla Ltd.
 * Copyright (C) 2021 Slava Monich <slava.monich@jolla.com>
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

#include "nci_log.h"

GLOG_MODULE_DEFINE("nci");

#if GUTIL_LOG_DEBUG
void
nci_dump_invalid_config_params(
    guint nparams,
    const GUtilData* params)
{
    /*
     * [NFCForum-TS-NCI-1.0]
     * 4.3.2 Retrieve the Configuration
     *
     * If the DH tries to retrieve any parameter(s) that
     * are not available in the NFCC, the NFCC SHALL respond
     * with a CORE_GET_CONFIG_RSP with a Status field of
     * STATUS_INVALID_PARAM, containing each unavailable
     * Parameter ID with a Parameter Len field of value zero.
     * In this case, the CORE_GET_CONFIG_RSP SHALL NOT include
     * any parameter(s) that are available on the NFCC.
     */
    if (GLOG_ENABLED(GLOG_LEVEL_DEBUG)) {
        const guint8* ptr = params->bytes;
        const guint8* end = ptr + params->size;
        GString* buf = g_string_new(NULL);
        guint i;

        for (i = 0; i < nparams && (ptr + 1) < end; i++, ptr += 2) {
            g_string_append_printf(buf, " %02x", *ptr);
        }
        GDEBUG("%c CORE_GET_CONFIG_RSP invalid parameter(s):%s", DIR_IN,
            buf->str);
        g_string_free(buf, TRUE);
    }
}
#endif

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
