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

#include <stdint.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

//Remove
#include <stdio.h>

#include "backend_event_loop.h"

struct backend_event_loop* backend_event_loop_create()
{
    struct backend_event_loop *del = calloc(sizeof(struct backend_event_loop), 1);

    if(!del)
        return NULL;

    if((del->efd = epoll_create(MAX_EPOLL_EVENTS)) == -1){
        free(del);
        return NULL;
    }

    LIST_INIT(&(del->timeout_list));

    return del;
}

void backend_configure_epoll_handle(struct backend_epoll_handle *handle,
		void *ptr, int fd, backend_epoll_cb cb)
{
	handle->data = ptr;
	handle->fd = fd;
	handle->cb = cb;
}

struct backend_epoll_handle* backend_create_epoll_handle(
        void *ptr, int fd, backend_epoll_cb cb){
    struct backend_epoll_handle *handle =
        calloc(sizeof(struct backend_epoll_handle), 1);

    if(handle != NULL)
		backend_configure_epoll_handle(handle, ptr, fd, cb);

    return handle;
}

int32_t backend_event_loop_update(struct backend_event_loop *del, uint32_t events,
        int32_t op, int32_t fd, void *ptr)
{
    struct epoll_event ev;

    ev.events = events;
    ev.data.ptr = ptr;

    return epoll_ctl(del->efd, op, fd, &ev);
} 

void backend_insert_timeout(struct backend_event_loop *del,
                            struct backend_timeout_handle *handle)
{
    struct backend_timeout_handle *itr = del->timeout_list.lh_first, *prev_itr;

    if (itr == NULL ||
        handle->timeout_clock < itr->timeout_clock) {
        LIST_INSERT_HEAD(&(del->timeout_list), handle, timeout_next);
        return;
    } 

    //Move to a separate insert function
    for (; itr != NULL; itr = itr->timeout_next.le_next) {
        if (handle->timeout_clock < itr->timeout_clock)
            break;

        prev_itr = itr;
    }

    LIST_INSERT_AFTER(prev_itr, handle, timeout_next);
}

//Added
void backend_remove_timeout(struct backend_timeout_handle *timeout)
{
    LIST_REMOVE(timeout, timeout_next);
    timeout->timeout_next.le_next = NULL;
    timeout->timeout_next.le_prev = NULL;
}

struct backend_timeout_handle* backend_event_loop_create_timeout(
        uint64_t timeout_clock, backend_timeout_cb timeout_cb, void *ptr,
        uint32_t intvl)
{
    //In an improved version, handle can be passed as argument so that it is up
    //to application how to allocate it
    struct backend_timeout_handle *handle =
        calloc(sizeof(struct backend_timeout_handle), 1);

    if (!handle)
        return NULL;

    handle->timeout_clock = timeout_clock;
    handle->cb = timeout_cb;
    handle->data = ptr;
    handle->intvl = intvl;

    return handle;
}

static void backend_event_loop_run_timers(struct backend_event_loop *del)
{
    struct backend_timeout_handle *timeout = del->timeout_list.lh_first;
    struct backend_timeout_handle *cur_timeout;
    struct timeval tv;
    uint64_t cur_time;

    gettimeofday(&tv, NULL);
    cur_time = (tv.tv_sec * 1e3) + (tv.tv_usec / 1e3);

    while (timeout != NULL) {
        if (timeout->timeout_clock <= cur_time) {
            cur_timeout = timeout;
            timeout = timeout->timeout_next.le_next;

            //Execute and remove timeout from list
            cur_timeout->cb(cur_timeout->data);
            backend_remove_timeout(cur_timeout);

            //Rearm timer if needed
            if (cur_timeout->intvl) {
                cur_timeout->timeout_clock = cur_time + cur_timeout->intvl;
                backend_insert_timeout(del, cur_timeout);
            }
        } else {
            break;
        }
    }
}

void backend_event_loop_run(struct backend_event_loop *del)
{
    struct backend_epoll_handle *cur_handle = NULL;
    struct epoll_event events[MAX_EPOLL_EVENTS];
    int nfds, i, sleep_time;

    struct timeval tv;
    uint64_t cur_time;
    struct backend_timeout_handle *timeout;

    while(1){
        timeout = del->timeout_list.lh_first;
        gettimeofday(&tv, NULL);
        cur_time = (tv.tv_sec * 1e3) + (tv.tv_usec / 1e3);
       
        if (timeout != NULL) {
            if (cur_time > timeout->timeout_clock)
                sleep_time = 0;
            else
                sleep_time = timeout->timeout_clock - cur_time;
        } else {
            sleep_time = -1;
        }

		nfds = epoll_wait(del->efd, events, MAX_EPOLL_EVENTS, sleep_time);

		if (nfds < 0)
			continue;

        //TODO: Make sure the order of processing is safe wrt event caching and
        //so on. I can't think of any problems right now, since we will not for
        //example free a device in the internal libusb_list. So a USB event will
        //always work as intended, only difference is that event might be
        //removed from list, but our code should handle that

        //No callbacks have been called between last timeout check and here, so
        //I can recycle timeout value
        if (timeout != NULL)
            backend_event_loop_run_timers(del);

        for(i=0; i<nfds; i++) {
            cur_handle = events[i].data.ptr;
            cur_handle->cb(cur_handle->data, cur_handle->fd, events[i].events);
        }

        if (del->itr_cb != NULL)
            del->itr_cb(del->itr_data);
    }
}
