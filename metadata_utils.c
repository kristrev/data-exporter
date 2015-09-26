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
#include <string.h>
#include <stdlib.h>

#include "metadata_utils.h"

int16_t metadata_utils_get_csv_pos(char *csv, uint8_t pos)
{
    char *start_token = csv;
    char *end_token = NULL;
    uint8_t org_csv_len = strlen(csv), i = 0;
    int16_t values [] = {-1, -1, -1, -1}, retval = -1;

    if (pos >= 4)
        return retval;

    while ((end_token = strstr(start_token, ",")) != NULL) {
        if (start_token != end_token) {
            *end_token = '\0';
            values[i] = atoi(start_token);
        }

        start_token = end_token + 1;

        if (++i == 4)
            return retval;
    } 

    //There is no point doing the length check here. If there are too many
    //elements, we will abort before we get here (since that element will for
    //example be the last one)
    if (start_token != (csv + org_csv_len))
        values[i] = atoi(start_token);

    if (i != 3)
        return retval;

    //Repair string by inserting delimiters again
    for (i=0; i<org_csv_len; i++) {
        if (csv[i] == '\0')
            csv[i] = ',';
    }

    return values[pos];
}
