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

#ifndef METADATA_EXPORTER_LOG_H
#define METADATA_EXPORTER_LOG_H

#include <stdio.h>
#include <time.h>

#define META_LOG_PREFIX "[%.2d:%.2d:%.2d %.2d/%.2d/%d]: "
#define META_PRINT2(fd, ...){fprintf(fd, __VA_ARGS__);fflush(fd);}

//The ## is there so that I dont have to fake an argument when I use the macro
//on string without arguments!
#define META_PRINT(fd, _fmt, ...) \
    do { \
        time_t rawtime; \
        struct tm *curtime; \
        time(&rawtime); \
        curtime = gmtime(&rawtime); \
        META_PRINT2(fd, META_LOG_PREFIX _fmt, curtime->tm_hour, \
                curtime->tm_min, curtime->tm_sec, curtime->tm_mday, \
                curtime->tm_mon + 1, 1900 + curtime->tm_year, \
                ##__VA_ARGS__);} while(0)

#endif
