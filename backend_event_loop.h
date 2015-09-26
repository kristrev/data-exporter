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

#ifndef BACKEND_EVENT_LOOP_H
#define BACKEND_EVENT_LOOP_H

#include <sys/queue.h>
#include <sys/epoll.h>

#define MAX_EPOLL_EVENTS 10

//Any resource used by the callback is stored in the implementing "class".
//Assume one separate callback function per type of event
//fd is convenient in the case where I use the same handler for two file
//descriptors, for example netlink
typedef void(*backend_epoll_cb)(void *ptr, int32_t fd, uint32_t events);
typedef void(*backend_timeout_cb)(void *ptr);
typedef backend_timeout_cb backend_itr_cb;

struct backend_epoll_handle{
    void *data;
    int32_t fd;
    backend_epoll_cb cb;
};

//timeout_clock is first timeout in wallclock (ms), intvl is frequency after
//that. Set to 0 if no repeat is needed
struct backend_timeout_handle{
    uint64_t timeout_clock;
    backend_timeout_cb cb;
    LIST_ENTRY(backend_timeout_handle) timeout_next;
    uint32_t intvl;
    void *data;
};

struct backend_event_loop{
    int32_t efd;
    LIST_HEAD(timeout, backend_timeout_handle) timeout_list;
    backend_itr_cb itr_cb;
    void *itr_data;
};

//Create an backend_event_loop struct
//TODO: Currently, allocations are made from heap. Add support for using
//deciding how the struct should be allocated. This also applies to
//backend_create_epoll_handle()
struct backend_event_loop* backend_event_loop_create();

//Update file descriptor + ptr to efd in events according to op
int32_t backend_event_loop_update(struct backend_event_loop *del, uint32_t events,
        int32_t op, int32_t fd, void *ptr);

//Insert timeout into list, we need manual control of adding timeouts
void backend_insert_timeout(struct backend_event_loop *del,
                            struct backend_timeout_handle *handle);
void backend_remove_timeout(struct backend_timeout_handle *timeout);

//Add a timeout which is controlled by main loop
struct backend_timeout_handle* backend_event_loop_create_timeout(
        uint64_t timeout_clock, backend_timeout_cb timeout_cb, void *ptr,
        uint32_t intvl);

//Fill handle with ptr, fd, and cb. Used by create_epoll_handle and can be used
//by applications that use a different allocater for handle
void backend_configure_epoll_handle(struct backend_epoll_handle *handle,
		void *ptr, int fd, backend_epoll_cb cb);

//Create (allocate) a new epoll handle and return it
struct backend_epoll_handle* backend_create_epoll_handle(void *ptr, int fd,
        backend_epoll_cb cb);

//Run event loop described by efd. Let it be up to the user how efd shall be
//stored
//Function is for now never supposed to return. If it returns, something has
//failed. Thus, I dont need a return value (yet)
void backend_event_loop_run(struct backend_event_loop *del);
#endif
