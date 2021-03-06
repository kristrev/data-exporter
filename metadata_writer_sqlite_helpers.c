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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include "metadata_exporter.h"
#include "metadata_writer_sqlite.h"
#include "metadata_writer_sqlite_helpers.h"
#include "metadata_exporter_log.h"

uint8_t md_writer_helpers_copy_db(char *prefix, size_t prefix_len,
        dump_db_cb dump_db, struct md_writer_sqlite *mws,
        delete_db_cb delete_db)
{
    int32_t output_fd;
    FILE *output;
    //TODO: Specify prefix from command line
    char dst_filename[128];

    memset(prefix + prefix_len, 'X', 6);
    output_fd = mkstemp(prefix);

    if (output_fd == -1) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Could not create temporary filename. Error: %s\n", strerror(errno));
        return RETVAL_FAILURE;
    }

    snprintf(dst_filename, 128, "%s_%d.json", prefix, mws->node_id);

    output = fdopen(output_fd, "w");

    if (!output) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Could not open random file as FILE*. Error: %s\n", strerror(errno));
        remove(prefix);
        return RETVAL_FAILURE;
    }

    if (dump_db(mws, output)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Failured to dump DB\n");
        remove(prefix);
        fclose(output);
        return RETVAL_FAILURE;
    }

    fclose(output);
    META_PRINT_SYSLOG(mws->parent, LOG_INFO, "Done with tmpfile %s\n", dst_filename);

    if (link(prefix, dst_filename) ||
        unlink(prefix)) {
        META_PRINT_SYSLOG(mws->parent, LOG_ERR, "Could not link/unlink dump-file: %s\n", strerror(errno));
        remove(prefix);
        remove(dst_filename);
        return RETVAL_FAILURE;
    }

    if (!delete_db)
        return RETVAL_SUCCESS;

    if (!delete_db(mws))
        return RETVAL_SUCCESS;

    //TODO: Decide what to do here! It is not really critical (content is
    //dumped to file and we handle multiple inserts), but we transfer
    //redundant data
    META_PRINT_SYSLOG(mws->parent, LOG_ERR, "DELETE failed\n");
    remove(dst_filename);
    return RETVAL_FAILURE;
}
