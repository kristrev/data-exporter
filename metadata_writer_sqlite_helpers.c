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

#include "metadata_exporter.h"
#include "metadata_writer_sqlite.h"
#include "metadata_writer_sqlite_helpers.h"
#include "metadata_exporter_log.h"

uint8_t md_sqlite_helpers_dump_write(sqlite3_stmt *stmt, FILE *output)
{
    int32_t retval = sqlite3_step(stmt);
    int32_t column_count, i = 0;

    while (retval == SQLITE_ROW) {
        column_count = sqlite3_column_count(stmt);

        if (fprintf(output, "%s", sqlite3_column_text(stmt, 0)) < 0)
            return RETVAL_FAILURE;

        for (i = 1; i<column_count; i++) {
            if (fprintf(output, ",%s", sqlite3_column_text(stmt, i)) < 0)
                return RETVAL_FAILURE;
        }

        if (fprintf(output, ";\n") < 0)
            return RETVAL_FAILURE;

        retval = sqlite3_step(stmt);
    }

    if (retval != SQLITE_DONE)
        return RETVAL_FAILURE;
    else
        return RETVAL_SUCCESS;
}

uint8_t md_writer_helpers_copy_db(char *prefix, size_t prefix_len,
        dump_db_cb dump_db, struct md_writer_sqlite *mws,
        sqlite3_stmt *delete_stmt)
{
    int32_t output_fd, retval;
    FILE *output;
    //TODO: Specify prefix from command line
    char dst_filename[128];

    memset(prefix + prefix_len, 'X', 6);
    output_fd = mkstemp(prefix);

    //Length check has already been made when we received command line argument,
    //so I know there is room
    snprintf(dst_filename, 128, "%s.sql", prefix);

    if (output_fd == -1) {
        META_PRINT(mws->parent->logfile, "Could not create temporary filename. Error: %s\n", strerror(errno));
        return RETVAL_FAILURE;
    }

    output = fdopen(output_fd, "w");

    if (!output) {
        META_PRINT(mws->parent->logfile, "Could not open random file as FILE*. Error: %s\n", strerror(errno));
        remove(prefix);
        return RETVAL_FAILURE;
    }

    if (dump_db(mws, output)) {
        META_PRINT(mws->parent->logfile, "Failured to dump DB\n");
        remove(prefix);
        fclose(output);
        return RETVAL_FAILURE;
    }

    fclose(output);
    META_PRINT(mws->parent->logfile, "Done with tmpfile\n");

    if (link(prefix, dst_filename) ||
        unlink(prefix)) {
        META_PRINT(mws->parent->logfile, "Could not link/unlink dump-file: %s\n", strerror(errno));
        remove(prefix);
        remove(dst_filename);
        return RETVAL_FAILURE;
    }

    sqlite3_reset(delete_stmt);
    retval = sqlite3_step(delete_stmt);

    if (retval != SQLITE_DONE) {
        //TODO: Decide what to do here! It is not really critical (content is
        //dumped to file and we handle multiple inserts), but we transfer
        //redundant data
        META_PRINT(mws->parent->logfile, "DELETE failed %s\n", sqlite3_errstr(retval));
        remove(dst_filename);
        return RETVAL_FAILURE;
    }

    return RETVAL_SUCCESS;
}
