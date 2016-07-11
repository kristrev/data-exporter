/* Copyright (c) 2015, Celerway, Tomasz Rozensztrauch <t.rozensztrauch@radytek.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

#include <sys/queue.h>
#include "metadata_exporter.h"

enum nne_type
{
    NNE_TYPE_INT8,
    NNE_TYPE_UINT8,
    NNE_TYPE_INT16,
    NNE_TYPE_UINT16,
    NNE_TYPE_INT32,
    NNE_TYPE_UINT32,
    NNE_TYPE_STRING
};

struct nne_value
{
    enum nne_type type;
    union
    {
        int8_t v_int8;
        uint8_t v_uint8;
        int16_t v_int16;
        uint16_t v_uint16;
        int32_t v_int32;
        uint32_t v_uint32;
        char *v_str;
    } u;
};

enum nne_metadata_idx
{
    NNE_IDX_MODE,
    NNE_IDX_SUBMODE,
    NNE_IDX_RSSI,
    NNE_IDX_RSCP,
    NNE_IDX_ECIO,
    NNE_IDX_RSRP,
    NNE_IDX_RSRQ,
    NNE_IDX_LAC,
    NNE_IDX_CID,
    NNE_IDX_OPER,
    __NNE_IDX_MAX
};

#define NNE_IDX_MAX (__NNE_IDX_MAX - 1)

struct nne_metadata
{
    uint64_t tstamp;
    const char *key;
    struct nne_value value;
};

struct nne_modem
{
    LIST_ENTRY(nne_modem) entries;
    uint64_t tstamp;
    uint32_t mccmnc;
    struct nne_metadata metadata[NNE_IDX_MAX + 1];
};


struct md_writer_nne {
    MD_WRITER;

    char directory[64];
    char node_id[16];
    uint32_t interval;
    char gps_prefix[16];
    uint32_t gps_instance_id;
    char gps_extension[16];
    char metadata_prefix[32];
    char metadata_extension[16];

    FILE *gps_file;
    uint32_t gps_sequence;
    char gps_fname[128];
    char gps_fname_tm[128];
    char metadata_fname[128];
    char metadata_fname_tm[128];
    struct json_object* metadata_cache;
    LIST_HEAD(nne_modem_list, nne_modem) modem_list;

    struct backend_timeout_handle *timeout_handle;
};

void md_nne_setup(struct md_exporter *mde, struct md_writer_nne* mwn);
void md_nne_usage();
