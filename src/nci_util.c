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

#include "nci_util_p.h"
#include "nci_log.h"

#include <gutil_macros.h>
#include <gutil_misc.h>

gboolean
nci_nfcid1_dynamic(
    const NciNfcid1* id)
{
    /*
     * As specified in [DIGITAL], in case of a single size NFCID1 (4 Bytes),
     * a value of nfcid10 set to 08h indicates that nfcid11 to nfcid13 SHALL
     * be dynamically generated. In such a situation the NFCC SHALL ignore
     * the nfcid11 to nfcid13 values and generate them dynamically.
     */
    return !id->len || (id->len == 4 && id->bytes[0] == 0x08);
}

gboolean
nci_nfcid1_equal(
    const NciNfcid1* id1,
    const NciNfcid1* id2)
{
    if (id1->len == id2->len) {
        if (id1->len == 4 && id1->bytes[0] == 0x08) {
            /* Compare only the first byte */
            return id1->bytes[0] == id2->bytes[0];
        } else {
            /* Compare the whole thing */
            return !memcmp(id1->bytes, id2->bytes,
                MIN(id1->len, sizeof(id1->bytes)));
        }
    } else if (!id1->len) {
        return nci_nfcid1_dynamic(id2);
    } else if (!id2->len) {
        return nci_nfcid1_dynamic(id1);
    } else {
        return FALSE;
    }
}

gboolean
nci_listen_mode(
    NCI_MODE mode)
{
    switch (mode) {
    case NCI_MODE_PASSIVE_LISTEN_A:
    case NCI_MODE_PASSIVE_LISTEN_B:
    case NCI_MODE_PASSIVE_LISTEN_F:
    case NCI_MODE_ACTIVE_LISTEN_A:
    case NCI_MODE_ACTIVE_LISTEN_F:
    case NCI_MODE_PASSIVE_LISTEN_V:
        return TRUE;
    case NCI_MODE_PASSIVE_POLL_A:
    case NCI_MODE_PASSIVE_POLL_B:
    case NCI_MODE_PASSIVE_POLL_F:
    case NCI_MODE_ACTIVE_POLL_A:
    case NCI_MODE_ACTIVE_POLL_F:
    case NCI_MODE_PASSIVE_POLL_V:
        break;
    }
    /* Assume Poll by default */
    return FALSE;
}

static
gboolean
nci_parse_find_config_param(
    guint nparams,
    const GUtilData* params,
    guint8 id,
    GUtilData* value)
{
    const guint8* ptr = params->bytes;
    const guint8* end = ptr + params->size;

    /*
     * +-----------------------------------------+
     * | ID  | 1 | The identifier                |
     * | Len | 1 | The length of Val (m)         |
     * | Val | m | The value of the parameter    |
     * +-----------------------------------------+
     */
    while (nparams > 0 && (ptr + 2) <= end && (ptr + 2 + ptr[1]) <= end) {
        if (ptr[0] == id) {
            value->size = ptr[1];
            value->bytes = ptr + 2;
            return TRUE;
        }
        nparams--;
        ptr += 2 + ptr[1];
    }
    return FALSE;
}

guint
nci_parse_config_param_uint(
    guint nparams,
    const GUtilData* params,
    guint8 id,
    guint* value)
{
    GUtilData data;

    if (nci_parse_find_config_param(nparams, params, id, &data) &&
        data.size > 0 && data.size <= sizeof(guint)) {
        gsize i;

        /*
         * 1.11 Coding Conventions
         *
         * All values greater than 1 octet are sent and
         * received in Little Endian format.
         */
        for (*value = 0, i = 0; i < data.size; i++) {
            *value += ((guint)data.bytes[i]) << (8 * i);
        }
        return data.size;
    } else {
        return 0;
    }
}

gboolean
nci_parse_config_param_nfcid1(
    guint nparams,
    const GUtilData* params,
    guint8 id,
    NciNfcid1* value)
{
    GUtilData data;

    if (nci_parse_find_config_param(nparams, params, id, &data)) {
        switch (data.size) {
        case 4: case 7: case 10:
            /* NFCID1 can be 4, 7, or 10 bytes long */
            value->len = (guint8) data.size;
            memcpy(value->bytes, data.bytes, value->len);
            memset(value->bytes + value->len, 0, sizeof(value->bytes) -
                value->len);
            return TRUE;
        }
    }
    return FALSE;
}

const NciModeParam*
nci_parse_mode_param(
    NciModeParam* param,
    NCI_MODE mode,
    const guint8* bytes,
    guint len)
{
    switch (mode) {
    case NCI_MODE_ACTIVE_POLL_A:
        if (!len) {
            return NULL;
        }
        /* fallthrough */
    case NCI_MODE_PASSIVE_POLL_A:
        /*
         * [NFCForum-TS-NCI-1.0]
         * Table 54: Specific Parameters for NFC-A Poll Mode
         *
         * +=========================================================+
         * | Offset | Size | Description                             |
         * +=========================================================+
         * | 0      | 2    | SENS_RES Response                       |
         * | 2      | 1    | Length of NFCID1 Parameter (n)          |
         * | 3      | n    | NFCID1 (0, 4, 7, or 10 Octets)          |
         * | 3 + n  | 1    | SEL_RES Response Length (m)             |
         * | 4 + n  | m    | SEL_RES Response (0 or 1 Octet)         |
         * +=========================================================+
         */
        if (len >= 4) {
            NciModeParamPollA* ppa = &param->poll_a;

            memset(ppa, 0, sizeof(*ppa));
            ppa->sens_res[0] = bytes[0];
            ppa->sens_res[1] = bytes[1];
            ppa->nfcid1_len = bytes[2];
            if (ppa->nfcid1_len <= sizeof(ppa->nfcid1) &&
                len >= ppa->nfcid1_len + 4 &&
                len >= ppa->nfcid1_len + 4 +
                bytes[ppa->nfcid1_len + 3]) {
                memcpy(ppa->nfcid1, bytes + 3, ppa->nfcid1_len);
                ppa->sel_res_len = bytes[ppa->nfcid1_len + 3];
                if (ppa->sel_res_len) {
                    ppa->sel_res = bytes[ppa->nfcid1_len + 4];
                }
#if GUTIL_LOG_DEBUG
                if (GLOG_ENABLED(GLOG_LEVEL_DEBUG)) {
                    GString* buf = g_string_new(NULL);
                    guint i;

                    for (i = 0; i < ppa->nfcid1_len; i++) {
                        g_string_append_printf(buf, " %02x", ppa->nfcid1[i]);
                    }
                    GDEBUG("NFC-A");
                    GDEBUG("  PollA.sel_res = 0x%02x", ppa->sel_res);
                    GDEBUG("  PollA.nfcid1 =%s", buf->str);
                    g_string_free(buf, TRUE);
                }
#endif
                return param;
            }
        }
        GDEBUG("Failed to parse parameters for NFC-A poll mode");
        return NULL;
    case NCI_MODE_PASSIVE_POLL_B:
        /*
         * [NFCForum-TS-NCI-1.0]
         * Table 56: Specific Parameters for NFC-B Poll Mode
         *
         * +=========================================================+
         * | Offset | Size | Description                             |
         * +=========================================================+
         * | 0      | 1    | SENSB_RES Response Length n (11 or 12)  |
         * | 1      | n    | Bytes 2-12 or 13 of SENSB_RES Response  |
         * +=========================================================+
         *
         * [NFCForum-TS-DigitalProtocol-1.0]
         * Table 25: SENSB_RES Format
         *
         * +=========================================================+
         * | Byte 1 | Byte 2-5 | Byte 6-9         | Byte 10-12 or 13 |
         * +=========================================================+
         * | 50h    | NFCID0   | Application Data | Protocol Info    |
         * +=========================================================+
         *
         * Table 27: Protocol Info Format
         *
         * +=========================================================+
         * | Byte 1 | 8 bits | Bit Rate Capability                   |
         * +--------+--------+---------------------------------------+
         * | Byte 2 | 4 bits | FSCI                                  |
         * |        | 4 bits | Protocol_Type                         |
         * +--------+--------+---------------------------------------+
         * | Byte 3 | 4 bits | FWI                                   |
         * |        | 2 bits | ADC                                   |
         * |        | 2 bits | FO                                    |
         * +-------------------optional -----------------------------+
         * | Byte 4 | 4 bits | SFGI                                  |
         * |        | 4 bits | RFU                                   |
         * +=========================================================+
         *
         * Table 29: FSCI to FSC Conversion
         * +=========================================================+
         * | FSCI        | 0h  1h  2h  3h  4h  5h  6h  7h  8h  9h-Fh |
         * |-------------+-------------------------------------------+
         * | FSC (bytes) | 16  24  32  40  48  64  96  128 256 RFU   |
         * +=========================================================+
         */
        if (len >= 1 && bytes[0] >= 11) {
            NciModeParamPollB* ppb = &param->poll_b;
            const guint fsci = (bytes[10] >> 4);
            static const guint fsc_table[] = {
                16, 24, 32, 40, 48, 64, 96, 128, 256
            };

            memset(ppb, 0, sizeof(*ppb));
            memcpy(ppb->nfcid0, bytes + 1, 4);
            ppb->fsc = (fsci < G_N_ELEMENTS(fsc_table)) ?
                fsc_table[fsci] :
                fsc_table[G_N_ELEMENTS(fsc_table) - 1];
            memcpy(ppb->app_data, bytes + 5, 4);
            ppb->prot_info.size = bytes[0] - 8;
            ppb->prot_info.bytes = bytes + 9;

#if GUTIL_LOG_DEBUG
            if (GLOG_ENABLED(GLOG_LEVEL_DEBUG)) {
                GString *buf = g_string_new(NULL);
                guint i;

                for (i = 0; i < ppb->prot_info.size; i++) {
                    g_string_append_printf(buf, " %02x",
                        ppb->prot_info.bytes[i]);
                }
                GDEBUG("NFC-B");
                GDEBUG("  PollB.fsc = %u", ppb->fsc);
                GDEBUG("  PollB.nfcid0 = %02x %02x %02x %02x", ppb->nfcid0[0],
                    ppb->nfcid0[1], ppb->nfcid0[2], ppb->nfcid0[3]);
                GDEBUG("  PollB.AppData = %02x %02x %02x %02x",
                    ppb->app_data[0], ppb->app_data[1], ppb->app_data[2],
                    ppb->app_data[3]);
                GDEBUG("  PollB.ProtInfo =%s", buf->str);
                g_string_free(buf, TRUE);
            }
#endif
            return param;
        }
        GDEBUG("Failed to parse parameters for NFC-B poll mode");
        return NULL;
    case NCI_MODE_ACTIVE_POLL_F:
    case NCI_MODE_PASSIVE_POLL_F:
        /*
         * [NFCForum-TS-NCI-1.0]
         * Table 58: Specific Parameters for NFC-F Poll Mode
         *
         * +=========================================================+
         * | Offset | Size | Description                             |
         * +=========================================================+
         * | 0      | 1    | Bit Rate (1 = 212 kbps, 2 = 424 kbps)   |
         * | 1      | 1    | SENSB_REF Response Length n (16 or 18)  |
         * | 2      | n    | Bytes 2-17 of SENSF_RES                 |
         * +=========================================================+
         */
        if (len > 1 && bytes[1] >= 8 && (bytes[1] + 2) <= len) {
            NciModeParamPollF* pf = &param->poll_f;

            pf->bitrate = bytes[0];
            memcpy(pf->nfcid2, bytes + 2, 8);
            GDEBUG("NFC-F");
            GDEBUG("  PollF.bitrate = %u%s", pf->bitrate,
                (pf->bitrate == 1) ? " (212 kbps)" :
                (pf->bitrate == 2) ? " (424 kbps)" : "");
            GDEBUG("  PollF.nfcid2 = %02x %02x %02x %02x %02x %02x %02x %02x",
                pf->nfcid2[0], pf->nfcid2[1], pf->nfcid2[2], pf->nfcid2[3],
                pf->nfcid2[4], pf->nfcid2[5], pf->nfcid2[6], pf->nfcid2[7]);
            return param;
        }
        /* This does happen */
        GDEBUG("No parameters for NFC-F poll mode");
        return NULL;
    case NCI_MODE_ACTIVE_LISTEN_F:
    case NCI_MODE_PASSIVE_LISTEN_F:
        /*
         * [NFCForum-TS-NCI-1.0]
         * Table 59: Specific Parameters for NFC-F Listen Mode
         *
         * +=========================================================+
         * | Offset | Size | Description                             |
         * +=========================================================+
         * | 0      | 1    | Length of Local NFCID2 n (0 or 8)       |
         * | 1      | n    | NFCID2 generated by the Local NFCC      |
         * +=========================================================+
         */
        if (len > 0 && (bytes[0] + 1) <= len) {
            NciModeParamListenF* lf =  &param->listen_f;

            if (bytes[0] == 0) {
                memset(lf, 0, sizeof(*lf));
                return param;
            } else if (bytes[0] == 8) {
                lf->nfcid2.size = bytes[0];
                lf->nfcid2.bytes = bytes + 1;
                return param;
            }
        }
        /* This does happen */
        GDEBUG("No parameters for NFC-F listen mode");
        return NULL;
    case NCI_MODE_PASSIVE_POLL_V:
        /*
         * NFC Controller Interface (NCI)
         * Technical Specification
         * Version 2.0
         *
         * Table 74: Specific Parameters for NFC-V Poll Mode
         *
         * +==============================================================+
         * | Offset | Size | Description                                  |
         * +==============================================================+
         * | 0      | 1    | RES_FLAG - 1st Byte of INVENTORY_RES         |
         * | 1      | 1    | DSFID - 2nd Byte of INVENTORY_RES            |
         * | 2      | 8    | UID - 3rd Byte to last Byte of INVENTORY_RES |
         * +==============================================================+
         */
        if (len >= 10) {
            NciModeParamPollV* pv = &param->poll_v;

            pv->res_flag = bytes[0];
            pv->dsfid = bytes[1];
            memcpy(pv->uid, bytes + 2, 8);
            GDEBUG("NFC-V");
            GDEBUG("  PollV.res_flag = 0x%02x", pv->res_flag);
            GDEBUG("  PollV.dsfid = 0x%02x", pv->dsfid);
            GDEBUG("  PollV.uid = %02x %02x %02x %02x %02x %02x %02x %02x",
                pv->uid[0], pv->uid[1], pv->uid[2], pv->uid[3],
                pv->uid[4], pv->uid[5], pv->uid[6], pv->uid[7]);
           return param;
        }
        GDEBUG("Failed to parse parameters for NFC-V poll mode");
        return NULL;
    case NCI_MODE_PASSIVE_LISTEN_V:
        break;
    case NCI_MODE_PASSIVE_LISTEN_A:
    case NCI_MODE_PASSIVE_LISTEN_B:
    case NCI_MODE_ACTIVE_LISTEN_A:
        /* NCI 1.0 defines no parameters for A/B Listen modes */
        return NULL;
    }
    GDEBUG("Unhandled activation mode 0x%02x", mode);
    return NULL;
}

gboolean
nci_parse_discover_ntf(
    NciDiscoveryNtf* ntf,
    NciModeParam* param,
    const guint8* pkt,
    guint len)
{
    /*
     * [NFCForum-TS-NCI-1.0]
     * Table 52: Control Messages to Start Discovery
     *
     * RF_DISCOVER_NTF
     *
     * +=========================================================+
     * | Offset | Size | Description                             |
     * +=========================================================+
     * | 0      | 1    | RF Discovery ID                         |
     * | 1      | 1    | RF Protocol                             |
     * | 2      | 1    | Activation RF Technology and Mode       |
     * | 3      | 1    | Length of RF Technology Parameters (n)  |
     * | 4      | n    | RF Technology Specific Parameters       |
     * | 4 + n  | 1    | Notification Type                       |
     * |        |      |-----------------------------------------|
     * |        |      | 0 | Last Notification                   |
     * |        |      | 1 | Last Notification (limit reached)   |
     * |        |      | 2 | More Notification to follow         |
     * +=========================================================+
     */
    if (len >= 5) {
        const guint n = pkt[3];

        if (len >= 5 + n) {
            ntf->discovery_id = pkt[0];
            ntf->protocol = pkt[1];
            ntf->mode = pkt[2];
            ntf->param_len = n;
            ntf->param_bytes = n ? (pkt + 4) : NULL;
            ntf->last = (pkt[4 + n] != 2 /* More to follow */);

#if GUTIL_LOG_DEBUG
            GDEBUG("RF_DISCOVER_NTF%s", ntf->last ? " (Last)" : "");
            GDEBUG("  RF Discovery ID = 0x%02x", ntf->discovery_id);
            GDEBUG("  RF Protocol = 0x%02x", ntf->protocol);
            GDEBUG("  Activation RF Mode = 0x%02x", ntf->mode);
            if (n && GLOG_ENABLED(GLOG_LEVEL_DEBUG)) {
                const guint8* bytes = ntf->param_bytes;
                GString* buf = g_string_new(NULL);
                guint i;

                for (i = 0; i < n; i++) {
                    g_string_append_printf(buf, " %02x", bytes[i]);
                }
                GDEBUG("  RF Tech Parameters =%s", buf->str);
                g_string_free(buf, TRUE);
            }
#endif /* GUTIL_LOG_DEBUG */

            if (ntf->param_bytes && param) {
                ntf->param = nci_parse_mode_param(param, ntf->mode,
                    ntf->param_bytes, n);
            } else {
                ntf->param = NULL;
            }
            return TRUE;
        }
    }
    GDEBUG("Failed to parse RF_DISCOVER_NTF");
    return FALSE;
}

static
gboolean
nci_parse_iso_dep_poll_a_param(
    NciActivationParamIsoDepPollA* param,
    const guint8* bytes,
    guint len)
{
    /* Answer To Select */
    const guint8 ats_len = bytes[0];

    /*
     * [NFCForum-TS-NCI-1.0]
     * Table 76: Activation Parameters for NFC-A/ISO-DEP Poll Mode
     *
     * +=========================================================+
     * | Offset | Size | Description                             |
     * +=========================================================+
     * | 0      | 1    | RATS Response Length (n)                |
     * | 1      | n    | RATS Response starting from Byte 2      |
     * +=========================================================+
     */
    if (ats_len >= 1 && len >= ats_len + 1) {
        const guint8* ats = bytes + 1;
        const guint8* ats_end = ats + ats_len;
        const guint8* ats_ptr = ats;
        const guint8* ta = NULL;
        const guint8* tb = NULL;
        const guint8* tc = NULL;
        const guint8 t0 = *ats_ptr++;

        if (t0 & NFC_T4A_ATS_T0_A) ta = ats_ptr++;
        if (t0 & NFC_T4A_ATS_T0_B) tb = ats_ptr++;
        if (t0 & NFC_T4A_ATS_T0_C) tc = ats_ptr++;
        if (ats_ptr <= ats_end) {
            /*
             * [NFCForum-TS-DigitalProtocol-1.0]
             * Table 66: FSCI to FSC Conversion
             */
            const guint8 fsci = (t0 & NFC_T4A_ATS_T0_FSCI_MASK);
            static const guint fsc_table[] = {
                16, 24, 32, 40, 48, 64, 96, 128, 256
            };

            /* Save TA, TB and TC if they are present */
            param->t0 = t0;
            if (ta) param->ta = *ta;
            if (tb) param->tb = *tb;
            if (tc) param->tc = *tc;
            param->fsc = (fsci < G_N_ELEMENTS(fsc_table)) ?
                fsc_table[fsci] :
                fsc_table[G_N_ELEMENTS(fsc_table) - 1];
            if ((param->t1.size = ats_end - ats_ptr) > 0) {
                param->t1.bytes = ats_ptr;
            }
#if GUTIL_LOG_DEBUG
            if (GLOG_ENABLED(GLOG_LEVEL_DEBUG)) {
                GDEBUG("ISO-DEP");
                GDEBUG("  FSC = %u", param->fsc);
                if (param->t1.bytes) {
                    GString* buf = g_string_new(NULL);
                    guint i;

                    for (i = 0; i < param->t1.size; i++) {
                        g_string_append_printf(buf, " %02x",
                            param->t1.bytes[i]);
                    }
                    GDEBUG("  T1 =%s", buf->str);
                    g_string_free(buf, TRUE);
                }
                GDEBUG("  T0 = 0x%02x", param->t0);
                if (param->t0 & NFC_T4A_ATS_T0_A) {
                    GDEBUG("  TA = 0x%02x", param->ta);
                }
                if (param->t0 & NFC_T4A_ATS_T0_B) {
                    GDEBUG("  TB = 0x%02x", param->tb);
                }
                if (param->t0 & NFC_T4A_ATS_T0_C) {
                    GDEBUG("  TC = 0x%02x", param->tc);
                }
            }
#endif
            return TRUE;
        }
    }
    return FALSE;
}

static
gboolean
nci_parse_iso_dep_listen_a_param(
    NciActivationParamIsoDepListenA* param,
    const guint8* bytes,
    guint len)
{
    /*
     * [NFCForum-TS-NCI-1.0]
     * Table 78: Activation Parameters for NFC-A/ISO-DEP Listen Mode
     *
     * +=========================================================+
     * | Offset | Size | Description                             |
     * +=========================================================+
     * | 0      | 1    | Byte 2 (PARAM) of the RATS Command      |
     * +=========================================================+
     */
    if (len > 0) {
        /*
         * [NFCForum-TS-DigitalProtocol-1.0]
         * Table 63: FSDI to FSD Conversion
         */
        const guint b2 = bytes[0];
        const guint8 fsdi = (b2 >> 4);
        static const guint fsd_table[] = {
            16, 24, 32, 40, 48, 64, 96, 128, 256
        };

        param->fsd = (fsdi < G_N_ELEMENTS(fsd_table)) ?
            fsd_table[fsdi] :
            fsd_table[G_N_ELEMENTS(fsd_table) - 1];
        param->did = b2 & 0x0f;

        GDEBUG("ISO-DEP");
        GDEBUG("  RatsCmd.fsd = %u", param->fsd);
        GDEBUG("  RatsCmd.did = %u", param->did);
        return TRUE;
    }
    return FALSE;
}

static
gboolean
nci_parse_iso_dep_poll_b_param(
    NciActivationParamIsoDepPollB* param,
    const guint8* bytes,
    guint len)
{
    /* [NFCForum-TS-NCI-1.0]
     * Table 75: Activation parameters for NFC-B/ISO-DEP Poll Mode
     *
     * +============================================================+
     * | Offset | Size | Description                                |
     * +============================================================+
     * | 0      | 1    | Length of ATTRIB Response Parameter (n)    |
     * | 1      | n    | ATTRIB Response                            |
     * +============================================================+
     */

    /* ATTRIB Response */
    const guint attrib_length = bytes[0];

    if (attrib_length >= 1) {
        /*
         * [NFCForum-TS-DigitalProtocol-1.0]
         * Table 79: ATTRIB Response Format
         */

#define NFC_T4B_MBLI_MASK (0xF0) /* MBLI Mask */
#define NFC_T4B_DID_MASK  (0x0F) /* DID Mask */

        /* First two octets of the first byte of ATTRIB Response */
        param->mbli = (bytes[1] & NFC_T4B_MBLI_MASK) >> 4;
        param->did  = (bytes[1] & NFC_T4B_DID_MASK);

        /* Higher Layer Response */
        if (attrib_length >= 2) {
            param->hlr.bytes = bytes + 2;
            param->hlr.size = attrib_length - 1;
        }

#if GUTIL_LOG_DEBUG
        if (GLOG_ENABLED(GLOG_LEVEL_DEBUG)) {
            GDEBUG("ISO-DEP");
            GDEBUG("  MBLI = %u", param->mbli);
            GDEBUG("  DID = %u", param->did);
            if (param->hlr.size > 0) {
                GString *buf = g_string_new(NULL);
                guint i;

                for (i = 0; i < param->hlr.size; i++) {
                    g_string_append_printf(buf, " %02x", bytes[i]);
                }
                GDEBUG("  HigherLayer Response =%s", buf->str);
                g_string_free(buf, TRUE);
            }
        }
#endif
        return TRUE;
    }
    return FALSE;
}

static
gboolean
nci_parse_iso_dep_listen_b_param(
    NciActivationParamIsoDepListenB* param,
    const guint8* bytes,
    guint len)
{
    /* [NFCForum-TS-NCI-1.0]
     * Table 79: Activation parameters for NFC-B/ISO-DEP Listen Mode
     *
     * +============================================================+
     * | Offset | Size | Description                                |
     * +============================================================+
     * | 0      | 1    | Length of ATTRIB Command Parameter (n)     |
     * | 1      | n    | ATTRIB Command starting with Byte 2        |
     * +============================================================+
     */

    /* ATTRIB Command */
    const guint cmdlen = bytes[0];

    if (cmdlen >= 8 && len > cmdlen) {
        /*
         * [NFCForum-TS-DigitalProtocol-1.0]
         * Table 71: ATTRIB Command Format
         */
        memcpy(param->nfcid0, bytes + 1, sizeof(param->nfcid0));
        memcpy(param->param, bytes + 5, 4);
        if (cmdlen > 8) {
            param->hlc.bytes = bytes + 9;
            param->hlc.size = cmdlen - 8;
        } else {
            memset(&param->hlc, 0, sizeof(param->hlc));
        }
#if GUTIL_LOG_DEBUG
        if (GLOG_ENABLED(GLOG_LEVEL_DEBUG)) {
            GString* buf = g_string_new(NULL);
            guint i;

            GDEBUG("ISO-DEP");
            for (i = 0; i < sizeof(param->nfcid0); i++) {
                g_string_append_printf(buf, " %02x", param->nfcid0[i]);
            }
            GDEBUG("  Attrib.nfcid0 =%s", buf->str);
            g_string_set_size(buf, 0);
            for (i = 0; i < sizeof(param->nfcid0); i++) {
                g_string_append_printf(buf, " %02x", param->param[i]);
            }
            GDEBUG("  Attrib.params =%s", buf->str);
            g_string_set_size(buf, 0);
            for (i = 0; i < param->hlc.size; i++) {
                g_string_append_printf(buf, " %02x", param->hlc.bytes[i]);
            }
            GDEBUG("  Attrib.hlc =%s", buf->str);
            g_string_free(buf, TRUE);
        }
#endif
        return TRUE;
    }
    return FALSE;
}

static
gboolean
nci_parse_nfc_dep_poll_param(
    NciActivationParamNfcDepPoll* param,
    const guint8* bytes,
    guint len)
{
    /* ATR_RES Length */
    const guint8 atr_res_len = bytes[0];

    /*
     * [NFCForum-TS-NCI-1.0]
     * Table 82: Activation Parameters for NFC-DEP Poll Mode
     *
     * +=========================================================+
     * | Offset | Size | Description                             |
     * +=========================================================+
     * | 0      | 1    | Length of ATR_RES Command Parameter (n) |
     * | 1      | n    | ATR_RES bytes from and including Byte 3 |
     * +=========================================================+
     */
    if (atr_res_len >= 15 && len >= atr_res_len + 1) {
        const guint8* atr_res = bytes + 1;

        /*
         * [NFCForum-TS-DigitalProtocol-1.0]
         * 14.6.3 ATR_RES Response
         */
        memcpy(param->nfcid3, atr_res, sizeof(param->nfcid3));
        param->did = atr_res[10];
        param->bs = atr_res[11];
        param->br = atr_res[12];
        param->to = atr_res[13];
        param->pp = atr_res[14];
        if (atr_res_len > 15) {
            param->g.bytes = atr_res + 15;
            param->g.size = atr_res_len - 15;
        }
#if GUTIL_LOG_DEBUG
        if (GLOG_ENABLED(GLOG_LEVEL_DEBUG)) {
            GString* buf = g_string_new(NULL);
            guint i;

            GDEBUG("NFC-DEP");
            for (i = 0; i < sizeof(param->nfcid3); i++) {
                g_string_append_printf(buf, " %02x", param->nfcid3[i]);
            }
            GDEBUG("  AtrRes.nfcid3 =%s", buf->str);
            GDEBUG("  AtrRes.did = 0x%02x", param->did);
            GDEBUG("  AtrRes.bs = 0x%02x", param->bs);
            GDEBUG("  AtrRes.br = 0x%02x", param->br);
            GDEBUG("  AtrRes.to = 0x%02x", param->to);
            GDEBUG("  AtrRes.pp = 0x%02x", param->pp);
            g_string_set_size(buf, 0);
            for (i = 0; i < param->g.size; i++) {
                g_string_append_printf(buf, " %02x", param->g.bytes[i]);
            }
            GDEBUG("  AtrRes.g =%s", buf->str);
            g_string_free(buf, TRUE);
        }
#endif
        return TRUE;
    }
    return FALSE;
}

static
gboolean
nci_parse_nfc_dep_listen_param(
    NciActivationParamNfcDepListen* param,
    const guint8* bytes,
    guint len)
{
    /* ATR_REQ Length */
    const guint8 atr_req_len = bytes[0];

    /*
     * [NFCForum-TS-NCI-1.0]
     * Table 83: Activation Parameters for NFC-DEP Listen Mode
     *
     * +=========================================================+
     * | Offset | Size | Description                             |
     * +=========================================================+
     * | 0      | 1    | Length of ATR_REQ Command Parameter (n) |
     * | 1      | n    | ATR_REQ bytes from and including Byte 3 |
     * +=========================================================+
     */
    if (atr_req_len >= 14 && len >= atr_req_len + 1) {
        const guint8* atr_req = bytes + 1;

        /*
         * [NFCForum-TS-DigitalProtocol-1.0]
         * 14.6.2 ATR_REQ Command
         */
        memcpy(param->nfcid3, atr_req, sizeof(param->nfcid3));
        param->did = atr_req[10];
        param->bs = atr_req[11];
        param->br = atr_req[12];
        param->pp = atr_req[13];
        if (atr_req_len > 14) {
            param->g.bytes = atr_req + 14;
            param->g.size = atr_req_len - 14;
        }
#if GUTIL_LOG_DEBUG
        if (GLOG_ENABLED(GLOG_LEVEL_DEBUG)) {
            GString* buf = g_string_new(NULL);
            guint i;

            GDEBUG("NFC-DEP");
            for (i = 0; i < sizeof(param->nfcid3); i++) {
                g_string_append_printf(buf, " %02x", param->nfcid3[i]);
            }
            GDEBUG("  AtrReq.nfcid3 =%s", buf->str);
            GDEBUG("  AtrReq.did = 0x%02x", param->did);
            GDEBUG("  AtrReq.bs = 0x%02x", param->bs);
            GDEBUG("  AtrReq.br = 0x%02x", param->br);
            GDEBUG("  AtrReq.pp = 0x%02x", param->pp);
            g_string_set_size(buf, 0);
            for (i = 0; i < param->g.size; i++) {
                g_string_append_printf(buf, " %02x", param->g.bytes[i]);
            }
            GDEBUG("  AtrReq.g =%s", buf->str);
            g_string_free(buf, TRUE);
        }
#endif
        return TRUE;
    }
    return FALSE;
}

static
const NciActivationParam*
nci_parse_activation_param(
    NciActivationParam* param,
    NCI_RF_INTERFACE intf,
    NCI_MODE mode,
    const guint8* bytes,
    guint len)
{
    switch (intf) {
    case NCI_RF_INTERFACE_ISO_DEP:
        switch (mode) {
        case NCI_MODE_PASSIVE_POLL_A:
        case NCI_MODE_ACTIVE_POLL_A:
            if (nci_parse_iso_dep_poll_a_param(&param->iso_dep_poll_a,
                bytes, len)) {
                return param;
            }
            GDEBUG("Failed to parse parameters for NFC-A/ISO-DEP poll mode");
            break;
        case NCI_MODE_PASSIVE_LISTEN_A:
        case NCI_MODE_ACTIVE_LISTEN_A:
            if (nci_parse_iso_dep_listen_a_param(&param->iso_dep_listen_a,
                bytes, len)) {
                return param;
            }
            GDEBUG("Failed to parse parameters for NFC-A/ISO-DEP listen mode");
            break;
        case NCI_MODE_PASSIVE_POLL_B:
            if (nci_parse_iso_dep_poll_b_param(&param->iso_dep_poll_b,
                bytes, len)) {
                return param;
            }
            GDEBUG("Failed to parse parameters for NFC-B/ISO-DEP poll mode");
            break;
        case NCI_MODE_PASSIVE_LISTEN_B:
            if (nci_parse_iso_dep_listen_b_param(&param->iso_dep_listen_b,
                bytes, len)) {
                return param;
            }
            GDEBUG("Failed to parse parameters for NFC-B/ISO-DEP listen mode");
            break;
        case NCI_MODE_PASSIVE_POLL_F:
        case NCI_MODE_ACTIVE_POLL_F:
        case NCI_MODE_PASSIVE_POLL_V:
        case NCI_MODE_PASSIVE_LISTEN_F:
        case NCI_MODE_ACTIVE_LISTEN_F:
        case NCI_MODE_PASSIVE_LISTEN_V:
            break;
        }
        break;
    case NCI_RF_INTERFACE_FRAME:
        /* There are no Activation Parameters for Frame RF interface */
        break;
    case NCI_RF_INTERFACE_NFC_DEP:
        switch (mode) {
        case NCI_MODE_ACTIVE_POLL_A:
        case NCI_MODE_ACTIVE_POLL_F:
        case NCI_MODE_PASSIVE_POLL_A:
        case NCI_MODE_PASSIVE_POLL_F:
            if (nci_parse_nfc_dep_poll_param(&param->nfc_dep_poll,
                bytes, len)) {
                return param;
            }
            GDEBUG("Failed to parse parameters for NFC-DEP poll mode");
            break;
        case NCI_MODE_ACTIVE_LISTEN_A:
        case NCI_MODE_ACTIVE_LISTEN_F:
        case NCI_MODE_PASSIVE_LISTEN_A:
        case NCI_MODE_PASSIVE_LISTEN_F:
            if (nci_parse_nfc_dep_listen_param(&param->nfc_dep_listen,
                bytes, len)) {
                return param;
            }
            GDEBUG("Failed to parse parameters for NFC-DEP listen mode");
            break;
        case NCI_MODE_PASSIVE_POLL_B:
        case NCI_MODE_PASSIVE_POLL_V:
        case NCI_MODE_PASSIVE_LISTEN_B:
        case NCI_MODE_PASSIVE_LISTEN_V:
            break;
        }
        break;
    case NCI_RF_INTERFACE_NFCEE_DIRECT:
    case NCI_RF_INTERFACE_PROPRIETARY:
        GDEBUG("Unhandled interface type");
        break;
    }
    return NULL;
}

gboolean
nci_parse_intf_activated_ntf(
    NciIntfActivationNtf* ntf,
    NciModeParam* mp,
    NciActivationParam* ap,
    const guint8* pkt,
    guint len)
{
    /*
     * [NFCForum-TS-NCI-1.0]
     * Table 61: Notification for RF Interface activation
     *
     * RF_INTF_ACTIVATED_NTF
     *
     * +=========================================================+
     * | Offset | Size | Description                             |
     * +=========================================================+
     * | 0      | 1    | RF Discovery ID                         |
     * | 1      | 1    | RF Interface                            |
     * | 2      | 1    | RF Protocol                             |
     * | 3      | 1    | Activation RF Technology and Mode       |
     * | 4      | 1    | Max Data Packet Payload Size            |
     * | 5      | 1    | Initial Number of Credits               |
     * | 6      | 1    | Length of RF Technology Parameters (n)  |
     * | 7      | n    | RF Technology Specific Parameters       |
     * | 7 + n  | 1    | Data Exchange RF Technology and Mode    |
     * | 8 + n  | 1    | Data Exchange Transmit Bit Rate         |
     * | 9 + n  | 1    | Data Exchange Receive Bit Rate          |
     * | 10 + n | 1    | Length of Activation Parameters (m)     |
     * | 11 + n | m    | Activation Parameters                   |
     * +=========================================================+
     */

    memset(ntf, 0, sizeof(*ntf));
    if (len > 6) {
        const guint n = pkt[6];
        const guint m = (len > (10 + n)) ? pkt[10 + n] : 0;

        if (len >= 11 + n + m) {
            ntf->discovery_id = pkt[0];
            ntf->rf_intf = pkt[1];
            ntf->protocol = pkt[2];
            ntf->mode = pkt[3];
            ntf->max_data_packet_size = pkt[4];
            ntf->num_credits = pkt[5];
            if ((ntf->mode_param_len = n) > 0) {
                ntf->mode_param_bytes = pkt + 7;
            }
            ntf->data_exchange_mode = pkt[7 + n];
            ntf->transmit_rate = pkt[8 + n];
            ntf->receive_rate = pkt[9 + n];
            if ((ntf->activation_param_len = m) > 0) {
                ntf->activation_param_bytes = pkt + (11 + n);
            }

#if GUTIL_LOG_DEBUG
            GDEBUG("RF_INTF_ACTIVATED_NTF");
            GDEBUG("  RF Discovery ID = 0x%02x", ntf->discovery_id);
            GDEBUG("  RF Interface = 0x%02x", ntf->rf_intf);
            if (ntf->rf_intf != NCI_RF_INTERFACE_NFCEE_DIRECT) {
                GDEBUG("  RF Protocol = 0x%02x", ntf->protocol);
                GDEBUG("  Activation RF Mode = 0x%02x", ntf->mode);
                GDEBUG("  Max Data Packet Size = %u",
                    ntf->max_data_packet_size);
                GDEBUG("  Initial Credits = %u", ntf->num_credits);
                if (n || m) {
                    if (GLOG_ENABLED(GLOG_LEVEL_DEBUG)) {
                        GString* buf = g_string_new(NULL);
                        guint i;

                        if (ntf->mode_param_len) {
                            const guint8* bytes = ntf->mode_param_bytes;

                            for (i = 0; i < ntf->mode_param_len; i++) {
                                g_string_append_printf(buf, " %02x", bytes[i]);
                            }
                            GDEBUG("  RF Tech Parameters =%s", buf->str);
                        }
                        GDEBUG("  Data Exchange RF Tech = 0x%02x",
                            ntf->data_exchange_mode);
                        if (ntf->activation_param_len) {
                            const guint8* bytes = ntf->activation_param_bytes;

                            g_string_set_size(buf, 0);
                            for (i = 0; i < ntf->activation_param_len; i++) {
                                g_string_append_printf(buf, " %02x", bytes[i]);
                            }
                            GDEBUG("  Activation Parameters =%s", buf->str);
                        }
                        g_string_free(buf, TRUE);
                    }
                } else {
                    GDEBUG("  Data Exchange RF Tech = 0x%02x",
                        ntf->data_exchange_mode);
                }
            }
#endif /* GUTIL_LOG_DEBUG */

            if (ntf->mode_param_bytes) {
                memset(mp, 0, sizeof(*mp));
                ntf->mode_param = nci_parse_mode_param(mp, ntf->mode,
                    ntf->mode_param_bytes, ntf->mode_param_len);
            }

            if (ntf->activation_param_bytes) {
                memset(ap, 0, sizeof(*ap));
                ntf->activation_param = nci_parse_activation_param(ap,
                    ntf->rf_intf, ntf->mode, ntf->activation_param_bytes,
                    ntf->activation_param_len);
            }
            return TRUE;
        }
    }
    GDEBUG("Failed to parse RF_INTF_ACTIVATED_NTF");
    return FALSE;
}

static
gsize
nci_mode_param_copy_impl(
    NciModeParam* dest,
    const NciModeParam* src,
    NCI_MODE mode)
{
    if (G_LIKELY(src) && G_LIKELY(dest)) {
        gsize size = sizeof(NciModeParam);
        const NciModeParamPollB* poll_b = NULL;
        const NciModeParamListenF* listen_f = NULL;

        switch (mode) {
        case NCI_MODE_ACTIVE_POLL_A:        /* fallthrough */
        case NCI_MODE_PASSIVE_POLL_A:
        case NCI_MODE_ACTIVE_POLL_F:
        case NCI_MODE_PASSIVE_POLL_F:
        case NCI_MODE_PASSIVE_POLL_V:
            memcpy(dest, src, size);
            return size;
        case NCI_MODE_PASSIVE_POLL_B:
            poll_b = &src->poll_b;
            memcpy(dest, src, size);
            if (poll_b->prot_info.bytes) {
                size = G_ALIGN8(size);
                guint8* ptr = (guint8*)dest + size;

                memcpy(ptr, poll_b->prot_info.bytes, poll_b->prot_info.size);
                dest->poll_b.prot_info.bytes = ptr;
                size += G_ALIGN8(poll_b->prot_info.size);
            }
            return size;
        case NCI_MODE_ACTIVE_LISTEN_F:
        case NCI_MODE_PASSIVE_LISTEN_F:
            listen_f = &src->listen_f;
            memcpy(dest, src, size);
            if (listen_f->nfcid2.bytes) {
                size = G_ALIGN8(size);
                guint8* ptr = (guint8*)dest + size;

                memcpy(ptr, listen_f->nfcid2.bytes, listen_f->nfcid2.size);
                dest->listen_f.nfcid2.bytes = ptr;
                size += G_ALIGN8(listen_f->nfcid2.size);
            }
            return size;
        case NCI_MODE_PASSIVE_LISTEN_V:
            break;
        case NCI_MODE_PASSIVE_LISTEN_A:     /* fallthrough */
        case NCI_MODE_PASSIVE_LISTEN_B:
        case NCI_MODE_ACTIVE_LISTEN_A:
            /* NCI 1.0 defines no parameters for A/B Listen modes */
            return 0;
        }
        GDEBUG("Unhandled activation mode 0x%02x", mode);
    }
    return 0;
}

gboolean
nci_parse_rf_deactivate_ntf(
    NciRfDeactivateNtf* ntf,
    const GUtilData* pkt)
{
    /*
     * [NFCForum-TS-NCI-1.0]
     * Table 62: Control Messages for RF Interface Deactivation
     *
     * RF_DEACTIVATE_NTF
     *
     * +=========================================================+
     * | Offset | Size | Description                             |
     * +=========================================================+
     * | 0      | 1    | Deactivation Type                       |
     * | 1      | 1    | Deactivation Reason                     |
     * +=========================================================+
     */
    if (pkt->size >= 2) {
        const guint8 type = pkt->bytes[0];
        const guint8 reason = pkt->bytes[1];

        switch (type) {
        case NCI_DEACTIVATE_TYPE_IDLE:
            GDEBUG("RF_DEACTIVATE_NTF Idle (%u)", reason);
            break;
        case NCI_DEACTIVATE_TYPE_DISCOVERY:
            GDEBUG("RF_DEACTIVATE_NTF Discovery (%u)", reason);
            break;
        case NCI_DEACTIVATE_TYPE_SLEEP:
            GDEBUG("RF_DEACTIVATE_NTF Sleep (%u)", reason);
            break;
        case NCI_DEACTIVATE_TYPE_SLEEP_AF:
            GDEBUG("RF_DEACTIVATE_NTF Sleep_AF (%u)", reason);
            break;
        default:
            GDEBUG("RF_DEACTIVATE_NTF %u (%u)", type, reason);
            return FALSE;
        }

        ntf->type = type;
        ntf->reason = reason;
        return TRUE;
    }
    GWARN("Failed to parse RF_DEACTIVATE_NTF");
    return FALSE;
}

static
gsize
nci_mode_param_size(
    const NciModeParam* param,
    NCI_MODE mode)
{
    gsize size = 0;

    if (G_LIKELY(param)) {
        const NciModeParamPollB* poll_b = NULL;
        const NciModeParamListenF* listen_f = NULL;

        switch (mode) {
        case NCI_MODE_ACTIVE_POLL_A:        /* fallthrough */
        case NCI_MODE_PASSIVE_POLL_A:
        case NCI_MODE_ACTIVE_POLL_F:
        case NCI_MODE_PASSIVE_POLL_F:
        case NCI_MODE_PASSIVE_POLL_V:
            size = sizeof(NciModeParam);
            break;
        case NCI_MODE_PASSIVE_POLL_B:
            size = sizeof(NciModeParam);
            poll_b = &param->poll_b;
            if (poll_b->prot_info.size) {
                size = G_ALIGN8(size);
                size += G_ALIGN8(poll_b->prot_info.size);
            }
            break;
        case NCI_MODE_ACTIVE_LISTEN_F:      /* fallthrough */
        case NCI_MODE_PASSIVE_LISTEN_F:
            size = sizeof(NciModeParam);
            listen_f = &param->listen_f;
            if (listen_f->nfcid2.size) {
                size = G_ALIGN8(size);
                size += G_ALIGN8(listen_f->nfcid2.size);
            }
            break;
        case NCI_MODE_PASSIVE_LISTEN_V:
        case NCI_MODE_PASSIVE_LISTEN_A:
        case NCI_MODE_PASSIVE_LISTEN_B:
        case NCI_MODE_ACTIVE_LISTEN_A:
            /* NCI 1.0 defines no parameters for A/B Listen modes */
            break;
        }
    }
    return size;
}

NciDiscoveryNtf*
nci_discovery_ntf_copy_array(
    const NciDiscoveryNtf* const* ntfs,
    guint count)
{
    if (G_LIKELY(count)) {
        guint i;
        gsize size = G_ALIGN8(sizeof(NciDiscoveryNtf)*count);
        NciDiscoveryNtf* copy;
        guint8* ptr;

        /* Calculate total size */
        for (i = 0; i < count; i++) {
            const NciDiscoveryNtf* src = ntfs[i];

            if (src->param_len) {
                size += G_ALIGN8(src->param_len);
                if (src->param) {
                    size += G_ALIGN8(nci_mode_param_size(src->param,
                        src->mode));
                }
            }
        }

        /* Deep copy of NciDiscoveryNtf */
        copy = g_malloc0(size);
        ptr = (void*)copy;
        ptr += G_ALIGN8(sizeof(NciDiscoveryNtf)*count);
        for (i = 0; i < count; i++) {
            const NciDiscoveryNtf* src = ntfs[i];
            NciDiscoveryNtf* dest = copy + i;

            *dest = *src;
            if (src->param_len) {
                dest->param_bytes = ptr;
                memcpy(ptr, src->param_bytes, src->param_len);
                ptr += G_ALIGN8(src->param_len);
                if (src->param) {
                    NciModeParam* dest_param = (NciModeParam*)ptr;
                    const gsize copied = nci_mode_param_copy_impl(dest_param,
                        src->param, src->mode);

                    if (copied) {
                        dest->param = dest_param;
                        ptr += G_ALIGN8(copied);
                    } else {
                        GWARN("ModeParam is not NULL but non copyable");
                        dest->param = NULL;
                    }
                }
            }
        }
        /* The result can be deallocated with a single g_free call */
        return copy;
    } else {
        return NULL;
    }
}

NciDiscoveryNtf*
nci_discovery_ntf_copy(
    const NciDiscoveryNtf* ntf)
{
    if (G_LIKELY(ntf)) {
        return nci_discovery_ntf_copy_array(&ntf, 1);
    } else {
        return NULL;
    }
}

/* Free the result with g_free() */
NciModeParam*
nci_util_copy_mode_param(
    const NciModeParam* param,
    NCI_MODE mode) /* Since 1.1.13 */
{
    if (G_LIKELY(param)) {
        NciModeParam* copy = NULL;
        gsize size = nci_mode_param_size(param, mode);

        switch (mode) {
        case NCI_MODE_ACTIVE_POLL_A:        /* fallthrough */
        case NCI_MODE_PASSIVE_POLL_A:
        case NCI_MODE_ACTIVE_POLL_F:
        case NCI_MODE_PASSIVE_POLL_F:
        case NCI_MODE_PASSIVE_POLL_B:
        case NCI_MODE_ACTIVE_LISTEN_F:
        case NCI_MODE_PASSIVE_LISTEN_F:
        case NCI_MODE_PASSIVE_POLL_V:
            if (size) {
                copy = g_malloc0(size);
                nci_mode_param_copy_impl(copy, param, mode);
            }
            return copy;
        case NCI_MODE_PASSIVE_LISTEN_V:
            break;
        case NCI_MODE_PASSIVE_LISTEN_A:     /* fallthrough */
        case NCI_MODE_PASSIVE_LISTEN_B:
        case NCI_MODE_ACTIVE_LISTEN_A:
            /* NCI 1.0 defines no parameters for A/B Listen modes */
            return NULL;
        }
        GDEBUG("Unhandled activation mode 0x%02x", mode);
    }
    return NULL;
}

/* Free the result with g_free() */
NciActivationParam*
nci_util_copy_activation_param(
    const NciActivationParam* param,
    NCI_RF_INTERFACE intf,
    NCI_MODE mode) /* Since 1.1.13 */
{
    if (G_LIKELY(param)) {
        NciActivationParam* copy = NULL;
        gsize size = sizeof(NciActivationParam);
        const NciActivationParamIsoDepPollA* iso_dep_poll_a = NULL;
        const NciActivationParamIsoDepPollB* iso_dep_poll_b = NULL;
        const NciActivationParamIsoDepListenB* iso_dep_listen_b = NULL;
        const NciActivationParamNfcDepPoll* nfc_dep_poll = NULL;
        const NciActivationParamNfcDepListen* nfc_dep_listen = NULL;

        switch (intf) {
        case NCI_RF_INTERFACE_ISO_DEP:
            switch (mode) {
            case NCI_MODE_PASSIVE_POLL_A:
            case NCI_MODE_ACTIVE_POLL_A:
                iso_dep_poll_a = &param->iso_dep_poll_a;
                if (iso_dep_poll_a->t1.size) {
                    size = G_ALIGN8(size);
                    size += G_ALIGN8(iso_dep_poll_a->t1.size);
                }
                copy = g_malloc0(size);
                memcpy(copy, param, sizeof(NciActivationParam));
                if (iso_dep_poll_a->t1.bytes) {
                    guint8* dest = (guint8*)copy +
                        G_ALIGN8(sizeof(NciActivationParam));
                    memcpy(dest, iso_dep_poll_a->t1.bytes,
                        iso_dep_poll_a->t1.size);
                    copy->iso_dep_poll_a.t1.bytes = dest;
                }
                return copy;
            case NCI_MODE_PASSIVE_LISTEN_A:
            case NCI_MODE_ACTIVE_LISTEN_A:
                return gutil_memdup(param, size);
            case NCI_MODE_PASSIVE_POLL_B:
                iso_dep_poll_b = &param->iso_dep_poll_b;
                if (iso_dep_poll_b->hlr.size) {
                    size = G_ALIGN8(size);
                    size += G_ALIGN8(iso_dep_poll_b->hlr.size);
                }
                copy = g_malloc0(size);
                memcpy(copy, param, sizeof(NciActivationParam));
                if (iso_dep_poll_b->hlr.bytes) {
                    guint8* dest = (guint8*)copy +
                        G_ALIGN8(sizeof(NciActivationParam));
                    memcpy(dest, iso_dep_poll_b->hlr.bytes,
                        iso_dep_poll_b->hlr.size);
                    copy->iso_dep_poll_b.hlr.bytes = dest;
                }
                return copy;
            case NCI_MODE_PASSIVE_LISTEN_B:
                iso_dep_listen_b = &param->iso_dep_listen_b;
                if (iso_dep_listen_b->hlc.size) {
                    size = G_ALIGN8(size);
                    size += G_ALIGN8(iso_dep_listen_b->hlc.size);
                }
                copy = g_malloc0(size);
                memcpy(copy, param, sizeof(NciActivationParam));
                if (iso_dep_listen_b->hlc.size) {
                    guint8* dest = (guint8*)copy +
                        G_ALIGN8(sizeof(NciActivationParam));
                    memcpy(dest, iso_dep_listen_b->hlc.bytes,
                        iso_dep_listen_b->hlc.size);
                    copy->iso_dep_listen_b.hlc.bytes = dest;
                }
                return copy;
            case NCI_MODE_PASSIVE_POLL_F:
            case NCI_MODE_ACTIVE_POLL_F:
            case NCI_MODE_PASSIVE_POLL_V:
            case NCI_MODE_PASSIVE_LISTEN_F:
            case NCI_MODE_ACTIVE_LISTEN_F:
            case NCI_MODE_PASSIVE_LISTEN_V:
                break;
            }
            break;
        case NCI_RF_INTERFACE_FRAME:
            /* There are no Activation Parameters for Frame RF interface */
            break;
        case NCI_RF_INTERFACE_NFC_DEP:
            switch (mode) {
            case NCI_MODE_ACTIVE_POLL_A:
            case NCI_MODE_ACTIVE_POLL_F:
            case NCI_MODE_PASSIVE_POLL_A:
            case NCI_MODE_PASSIVE_POLL_F:
                nfc_dep_poll = &param->nfc_dep_poll;
                size += G_ALIGN8(nfc_dep_poll->g.size);
                copy = g_malloc0(size);
                memcpy(copy, param, sizeof(NciActivationParam));
                if (nfc_dep_poll->g.bytes) {
                    guint8* dest = (guint8*)copy +
                        G_ALIGN8(sizeof(NciActivationParam));
                    memcpy(dest, nfc_dep_poll->g.bytes,
                        nfc_dep_poll->g.size);
                    copy->nfc_dep_poll.g.bytes = dest;
                }
                return copy;
            case NCI_MODE_ACTIVE_LISTEN_A:
            case NCI_MODE_ACTIVE_LISTEN_F:
            case NCI_MODE_PASSIVE_LISTEN_A:
            case NCI_MODE_PASSIVE_LISTEN_F:
                nfc_dep_listen = &param->nfc_dep_listen;
                size += G_ALIGN8(nfc_dep_listen->g.size);
                copy = g_malloc0(size);
                memcpy(copy, param, sizeof(NciActivationParam));
                if (nfc_dep_listen->g.bytes) {
                    guint8* dest = (guint8*)copy +
                        G_ALIGN8(sizeof(NciActivationParam));
                    memcpy(dest, nfc_dep_listen->g.bytes,
                        nfc_dep_listen->g.size);
                    copy->nfc_dep_listen.g.bytes = dest;
                }
                return copy;
            case NCI_MODE_PASSIVE_POLL_B:
            case NCI_MODE_PASSIVE_POLL_V:
            case NCI_MODE_PASSIVE_LISTEN_B:
            case NCI_MODE_PASSIVE_LISTEN_V:
                break;
            }
            break;
        case NCI_RF_INTERFACE_NFCEE_DIRECT:
        case NCI_RF_INTERFACE_PROPRIETARY:
            GDEBUG("Unhandled interface type");
            break;
        }
    }
    return NULL;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
