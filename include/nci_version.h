/*
 * Copyright (C) 2020-2025 Slava Monich <slava@monich.com>
 * Copyright (C) 2020-2021 Jolla Ltd.
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

#ifndef NCI_CORE_VERSION_H
#define NCI_CORE_VERSION_H

/* This file exists since version 1.0.6 */

/* libncicore version */
#define NCI_CORE_VERSION_MAJOR   1
#define NCI_CORE_VERSION_MINOR   1
#define NCI_CORE_VERSION_RELEASE 29

#define NCI_CORE_VERSION_WORD(v1,v2,v3) \
    ((((v1) & 0x7f) << 24) | \
     (((v2) & 0xfff) << 12) | \
      ((v3) & 0xfff))

#define NCI_CORE_VERSION_GET_MAJOR(v)  (((v) >> 24) & 0x7f)
#define NCI_CORE_VERSION_GET_MINOR(v)  (((v) >> 12) & 0xfff)
#define NCI_CORE_VERSION_GET_RELEASE(v) ((v) & 0xfff)

/* Current version as a single word */
#define NCI_CORE_VERSION \
   NCI_CORE_VERSION_WORD(NCI_CORE_VERSION_MAJOR, \
      NCI_CORE_VERSION_MINOR, NCI_CORE_VERSION_RELEASE)

/* Compatibility with libncicore <= 1.1.20 (RELEASE was called NANO) */
#define NCI_CORE_VERSION_NANO        NCI_CORE_VERSION_RELEASE
#define NCI_CORE_VERSION_GET_NANO(v) NCI_CORE_VERSION_GET_RELEASE(v)

/* Specific versions */
#define NCI_CORE_VERSION_1_1_27      NCI_CORE_VERSION_WORD(1,1,27)
#define NCI_CORE_VERSION_1_1_28      NCI_CORE_VERSION_WORD(1,1,28)
#define NCI_CORE_VERSION_1_1_29      NCI_CORE_VERSION_WORD(1,1,29)

#endif /* NCI_CORE_VERSION_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
