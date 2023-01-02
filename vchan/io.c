/*
 * The Qubes OS Project, http://www.qubes-os.org
 *
 * Copyright (C) 2010  Rafal Wojtczuk      <rafal@invisiblethingslab.com>
 * Copyright (C) 2013  Marek Marczykowski  <marmarek@invisiblethingslab.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */
#define XC_WANT_COMPAT_EVTCHN_API

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xenstore.h>
#include <xenctrl.h>
#include <poll.h>
#include "libvchan.h"
#include "libvchan_private.h"

/* check if domain is still alive */
int libvchan__check_domain_alive(xc_interface *xc_handle, int dom) {
    struct evtchn_status evst;
    xc_dominfo_t dominfo;
    int ret;

    /* first try using domctl, more reliable but available in a privileged
     * domain only */
    ret = xc_domain_getinfo(xc_handle, dom, 1, &dominfo);
    if (ret == 1)
        return dominfo.domid == (uint32_t)dom && !dominfo.dying;
    else if (ret == -1 && errno == ESRCH)
        return 0;
    /* otherwise fallback to xc_evtchn_status method */

    evst.dom = dom;
    /* xc_evtchn_status will return different error depending on
     * existence of "source" domain:
     * ESRCH - domain don't exists
     * EINVAL/EPERM - domain exsts but port is invalid / cannot check
     * its status
     */
    evst.port = -1;

    ret = xc_evtchn_status(xc_handle, &evst);
    if (ret == -1 && errno == ESRCH) {
        return 0;
    }
    return 1;
}

int libvchan_write(libvchan_t *ctrl, const void *data, size_t size) {
    return libxenvchan_write(ctrl->xenvchan, (char*)data, size);
}

int libvchan_send(libvchan_t *ctrl, const void *data, size_t size) {
    return libxenvchan_send(ctrl->xenvchan, (char*)data, size);
}

int libvchan_read(libvchan_t *ctrl, void *data, size_t size) {
    return libxenvchan_read(ctrl->xenvchan, (char*)data, size);
}

void libvchan_set_blocking(libvchan_t *ctrl, bool blocking) {
    ctrl->xenvchan->blocking = blocking;
}

int libvchan_recv(libvchan_t *ctrl, void *data, size_t size) {
    return libxenvchan_recv(ctrl->xenvchan, (char*)data, size);
}

int libvchan_wait(libvchan_t *ctrl) {
    int ret = -2; /* invalid, so can be distinguished from real
                     libxenvchan_wait return code */
    struct xs_handle *xs;

    if (ctrl->xenvchan->is_server && libxenvchan_is_open(ctrl->xenvchan) == 2) {
        /* In case of vchan server waiting for a client, we'll not receive any
         * notification if the remote domain dies before connecting. Because of
         * that, check periodically if remote domain is still alive while
         * waiting for a connection. Actually this doesn't cover all the cases
         * - if remote domain is still alive, but remote process dies before
         * connecting, we'll also not receive any notification. But this, in
         * most cases, can be solved by application using libvchan.
         *
         * During normal operation this shouldn't be long - in most cases vchan
         * client will connect almost instantly. So this sleep(10) loop will
         * not hurt. Alternativelly it could be implemented with
         * xs_watch("@releaseDomain"), but such approach will slow down most
         * common execution path (xs_open+xs_watch even if client connects
         * right away).
         */
        while (ret == -2 && libxenvchan_is_open(ctrl->xenvchan) == 2) {
            int vchan_fd = libxenvchan_fd_for_select(ctrl->xenvchan);
            struct pollfd fds[] = {
                { .fd = vchan_fd, .events = POLLIN, .revents = 0 },
            };
            switch (poll(fds, 1, 10000)) {
                case 0:
                    if (!libvchan__check_domain_alive(ctrl->xc_handle, ctrl->remote_domain))
                        return -1;
                    break;
                case 1:
                    /* break the loop */
                    ret = -1;
                    break;
                default:
                    if (errno == EINTR)
                        break;
                    perror("poll");
                    return -1;
            }
        }
    }
    ret = libxenvchan_wait(ctrl->xenvchan);
    if (ctrl->xs_path) {
        xs_transaction_t trans;
        char *xs_dir_path, *last_slash;
        char **dir_list;
        unsigned int dir_size;
        /* remove xenstore entry at first client connection */
        xs = xs_open(0);
        if (xs) {
            /* If xenstore connection failed just do not remove entries, but do
             * not abort whole function, especially still free the memory.
             *
             * If that was the last connection waiting to that domain,
             * remove the whole directory. Use transaction to avoid race
             * condition.
             */
            xs_dir_path = strdup(ctrl->xs_path);
            /* cut last directory component */
            last_slash = strrchr(xs_dir_path, '/');
            if (last_slash)
                *last_slash = '\0';
            do {
                trans = xs_transaction_start(xs);
                if (trans == XBT_NULL) {
                    perror("xs_transaction_start");
                    break;
                }
                xs_rm(xs, trans, ctrl->xs_path);
                dir_list = xs_directory(xs, trans, xs_dir_path, &dir_size);
                if (dir_list && dir_size == 0) {
                    /* that was the last entry, remove the whole directory */
                    xs_rm(xs, trans, xs_dir_path);
                }
                if (dir_list)
                    free(dir_list);
            } while (!xs_transaction_end(xs, trans, 0) && errno == EAGAIN);
            free(xs_dir_path);
            xs_close(xs);
        }
        free(ctrl->xs_path);
        ctrl->xs_path = NULL;
    }
    return ret;
}

void libvchan_close(libvchan_t *ctrl) {
    struct xs_handle *xs;

    libxenvchan_close(ctrl->xenvchan);
    if (ctrl->xenvchan && ctrl->xs_path) {
        /* remove server xenstore entry in case of no client connected */
        xs = xs_open(0);
        if (xs) {
            /* if xenstore connection failed just do not remove entries, but do
             * not abort whole function, especially still free the memory
             */
            xs_rm(xs, 0, ctrl->xs_path);
            xs_close(xs);
        }
        free(ctrl->xs_path);
    }
    /* this releases watches too */
    if (ctrl->xs)
        xs_close(ctrl->xs);
    if (ctrl->xc_handle)
        xc_interface_close(ctrl->xc_handle);
    free(ctrl);
}

EVTCHN libvchan_fd_for_select(libvchan_t *ctrl) {
    return libxenvchan_fd_for_select(ctrl->xenvchan);
}

int libvchan_data_ready(libvchan_t *ctrl) {
    return libxenvchan_data_ready(ctrl->xenvchan);
}

int libvchan_buffer_space(libvchan_t *ctrl) {
    return libxenvchan_buffer_space(ctrl->xenvchan);
}

int libvchan_is_open(libvchan_t *ctrl) {
    int ret;
    struct evtchn_status evst;

    ret = libxenvchan_is_open(ctrl->xenvchan);
    if (ret == 2) {
        if (!libvchan__check_domain_alive(ctrl->xc_handle, ctrl->remote_domain))
            return VCHAN_DISCONNECTED;
        return VCHAN_WAITING;
    }
    if (!ret)
        return VCHAN_DISCONNECTED;
    /* slow check in case of domain destroy */
    evst.port = ctrl->xenvchan->event_port;
    evst.dom = DOMID_SELF;
    if (xc_evtchn_status(ctrl->xc_handle, &evst)) {
        perror("xc_evtchn_status");
        return VCHAN_DISCONNECTED;
    }
    if (evst.status != EVTCHNSTAT_interdomain) {
        if (!ctrl->xenvchan->is_server)
            ctrl->xenvchan->ring->srv_live = 0;
        return VCHAN_DISCONNECTED;
    }
    return VCHAN_CONNECTED;
}
