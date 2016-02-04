/* Copyright (c) 2015, Celerway, Kristian Evensen <kristrev@celerway.com>
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#ifdef OPENWRT
#include <uci.h>
#endif

#include "system_helpers.h"
#include "metadata_exporter.h"
#include "metadata_exporter_log.h"

#ifdef OPENWRT
uint32_t system_helpers_get_nodeid()
{
    char *opt;
    struct uci_ptr uciptr;
    int retval;
    long node_id;
    struct uci_context *ctx = uci_alloc_context();

    if(ctx == NULL)
        return 0;

    opt = malloc(strlen(NODEIDPATH)+1);
    strcpy(opt, NODEIDPATH);
    memset(&uciptr, 0, sizeof(uciptr));

    retval = uci_lookup_ptr(ctx, &uciptr, opt, true);

    if (retval != UCI_OK || uciptr.o == NULL) {
        free(opt);
        uci_free_context(ctx);
        return 0;
    }

    node_id = atol(uciptr.o->v.string);
    free(opt);
    uci_free_context(ctx);

    if (node_id <= 0 || node_id > UINT32_MAX)
        return 0;

    return (uint32_t) node_id;
}
#elif MONROE
uint32_t system_helpers_get_nodeid()
{
    char num_buf[255];
    long node_id;
    size_t retval;
    //Check if file exists
    FILE *node_file = fopen("/etc/config/nodeid", "r");

    if (node_file == NULL)
        return 0;

    memset(num_buf, 0, sizeof(num_buf));
    retval = fread(num_buf, sizeof(char), 255, node_file);
    fclose(node_file);

    if (!retval)
        return 0;

    node_id = atol(num_buf);

    if (node_id <= 0 || node_id > UINT32_MAX)
        return 0;

    return (uint32_t) node_id;
}
#else
uint32_t system_helpers_get_nodeid(){
    return 0;
}
#endif

uint8_t system_helpers_check_address(const char *addr)
{
    struct addrinfo hint, *res;
    int32_t retval;

    memset(&hint, 0, sizeof(struct addrinfo));
    //ZeroMQ implementation does not use v6 yet
    hint.ai_family = AF_INET;

    retval = getaddrinfo(addr, NULL, &hint, &res); 

    //If address can't be resolved or there is something wrong with address,
    //getaddrinfo will fail. We don't care about the actual result.
    if (retval)
        return RETVAL_FAILURE;

    freeaddrinfo(res);
    return RETVAL_SUCCESS;
}
