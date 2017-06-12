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
#include <json-c/json.h>
#include <stdio.h>
#include <sqlite3.h>

#include "metadata_exporter.h"

uint8_t md_json_helpers_dump_write(sqlite3_stmt *stmt, json_object *jarray)
{
    int32_t retval;
    int32_t column_count, i = 0;

    while ((retval = sqlite3_step(stmt)) == SQLITE_ROW) {
        json_object *json = json_object_new_object();
        if (json == NULL)
            return RETVAL_FAILURE;

        column_count = sqlite3_column_count(stmt);

        for (i = 0; i < column_count; i++)
        {
            json_object *object = NULL;

            switch (sqlite3_column_type(stmt, i))
            {
                case SQLITE_INTEGER:
                    object = json_object_new_int64(sqlite3_column_int64(stmt, i));
                    break;

                case SQLITE_TEXT:
                    object = json_object_new_string((const char *)sqlite3_column_text(stmt, i));
                    break;

                case SQLITE_FLOAT:
                    object = json_object_new_double(sqlite3_column_double(stmt, i));
                    break;

                case SQLITE_NULL:
                    continue;
            }

            if (object == NULL) {
                json_object_put(json);
                return RETVAL_FAILURE;
            }

            json_object_object_add(json, sqlite3_column_name(stmt, i),object);
        }

        if (json_object_object_length(json) > 0)
            json_object_array_add(jarray, json);
        else
            json_object_put(json);
    }

    if (retval != SQLITE_DONE)
        return RETVAL_FAILURE;
    else
        return RETVAL_SUCCESS;
}
