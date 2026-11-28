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
#include <errno.h>
#include <unistd.h>
#ifndef CONFIG_STUBDOM
#include <sys/epoll.h>
#endif
#include "libvchan.h"
#include "libvchan_private.h"

/*
 * Check the remote domain with domctl: 1 alive, 0 dead or dying, -1 if
 * domctl is not available (only a privileged domain can use it).
 */
static int libvchan__domctl_alive(xc_interface *xc_handle, int dom) {
#ifdef HAVE_XC_DOMAIN_GETINFO_SINGLE
    xc_domaininfo_t dominfo;
    int ret = xc_domain_getinfo_single(xc_handle, dom, &dominfo);

    if (ret == 0)
        return !(dominfo.flags & XEN_DOMINF_dying);
#else
    xc_dominfo_t dominfo;
    int ret = xc_domain_getinfo(xc_handle, dom, 1, &dominfo);

    if (ret == 1)
        return dominfo.domid == (uint32_t)dom && !dominfo.dying;
#endif
    if (ret == -1 && errno == ESRCH)
        return 0;
    return -1;
}

/* check if domain is still alive */
int libvchan__check_domain_alive(xc_interface *xc_handle, int dom) {
    struct evtchn_status evst;
    int ret;

    ret = libvchan__domctl_alive(xc_handle, dom);
    if (ret >= 0)
        return ret;
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

static void libvchan__arm_death_watch(libvchan_t *ctrl);

/*
 * Xen sends no vchan event when the remote domain dies, so a wait for one
 * would never end. Watch @releaseDomain next to the event channel (not
 * @releaseDomain/<domid>: xenstored supports that only since Xen 4.22).
 * Only privileged callers get that event; elsewhere nothing changes.
 * libvchan_fd_for_select() returns the epoll fd; the watch joins it only
 * once a client is connected, see libvchan__arm_death_watch().
 */
void libvchan__watch_peer_death(libvchan_t *ctrl) {
#ifndef CONFIG_STUBDOM
    struct xs_handle *xs;
    struct epoll_event ev = { .events = EPOLLIN };
    int epoll_fd;

    if (libvchan__domctl_alive(ctrl->xc_handle, ctrl->remote_domain) != 1)
        return;
    xs = xs_open(0);
    if (!xs) {
        perror("xs_open");
        return;
    }
    if (!xs_watch(xs, "@releaseDomain", "release")) {
        fprintf(stderr, "Cannot setup @releaseDomain watch\n");
        goto err_xs;
    }
    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        goto err_xs;
    }
    ev.data.fd = libxenvchan_fd_for_select(ctrl->xenvchan);
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev)) {
        perror("epoll_ctl");
        goto err_epoll;
    }
    ctrl->death_xs = xs;
    ctrl->epoll_fd = epoll_fd;
    libvchan__arm_death_watch(ctrl);
    return;
err_epoll:
    close(epoll_fd);
err_xs:
    xs_close(xs);
#else
    (void)ctrl;
#endif
}

/*
 * Add the watch to the epoll fd once connected. A server waiting for its
 * client keeps it out: callers take a wakeup of that fd as "client
 * connected", and the watch also fires when any other domain dies.
 */
static void libvchan__arm_death_watch(libvchan_t *ctrl) {
#ifndef CONFIG_STUBDOM
    struct epoll_event ev = { .events = EPOLLIN };
    char **vec;

    if (!ctrl->death_xs || ctrl->death_watch_armed ||
            libxenvchan_is_open(ctrl->xenvchan) == 2)
        return;
    /* drop events queued so far (one fires right after xs_watch) */
    while ((vec = xs_check_watch(ctrl->death_xs)))
        free(vec);
    ev.data.fd = xs_fileno(ctrl->death_xs);
    if (epoll_ctl(ctrl->epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev)) {
        perror("epoll_ctl");
        return;
    }
    ctrl->death_watch_armed = true;
    if (!libvchan__check_domain_alive(ctrl->xc_handle, ctrl->remote_domain))
        ctrl->peer_dead = true;
#else
    (void)ctrl;
#endif
}

/*
 * Wait for a vchan event, or return -1 once the remote domain is dying. With
 * once, also return 0 after a watch event for a live domain (watches fire
 * spuriously) instead of waiting more.
 */
static int libvchan__wait_event(libvchan_t *ctrl, bool once) {
    struct pollfd fds[2];
    char **vec;

    if (ctrl->peer_dead)
        return -1;
    if (!ctrl->death_xs)
        return libxenvchan_wait(ctrl->xenvchan);
    libvchan__arm_death_watch(ctrl);
    if (ctrl->peer_dead)
        return -1;
    fds[0] = (struct pollfd) {
        .fd = libxenvchan_fd_for_select(ctrl->xenvchan), .events = POLLIN };
    fds[1] = (struct pollfd) {
        .fd = xs_fileno(ctrl->death_xs), .events = POLLIN };
    for (;;) {
        if (poll(fds, ctrl->death_watch_armed ? 2 : 1, -1) < 0) {
            /* poll() is never restarted by SA_RESTART, unlike read() */
            if (errno == EINTR)
                continue;
            perror("poll");
            return -1;
        }
        if (ctrl->death_watch_armed && fds[1].revents) {
            while ((vec = xs_check_watch(ctrl->death_xs)))
                free(vec);
            if (!libvchan__check_domain_alive(ctrl->xc_handle, ctrl->remote_domain)) {
                ctrl->peer_dead = true;
                return -1;
            }
        }
        if (fds[0].revents)
            return libxenvchan_wait(ctrl->xenvchan);
        if (once)
            return 0;
    }
}

/*
 * With the watch, the blocking paths below call libxenvchan only once it can
 * complete without waiting, and do the waiting in libvchan__wait_event(). A
 * failed wait ends the call only if what it waited for is still missing, so
 * data written just before the remote domain died is not lost.
 */

int libvchan_write(libvchan_t *ctrl, const void *data, size_t size) {
    struct libxenvchan *xenvchan = ctrl->xenvchan;
    size_t pos = 0;

    if (!xenvchan) {
        errno = EAGAIN;
        return -1;
    }
    if (!xenvchan->blocking || !ctrl->death_xs)
        return libxenvchan_write(xenvchan, (char*)data, size);
    if (!libxenvchan_is_open(xenvchan))
        return -1;
    while (pos < size) {
        int space, ret;

        if (!libxenvchan_is_open(xenvchan))
            return -1;
        space = libxenvchan_buffer_space(xenvchan);
        if (space == 0) {
            if (libvchan__wait_event(ctrl, false) &&
                    libxenvchan_buffer_space(xenvchan) == 0)
                return -1;
            continue;
        }
        if ((size_t)space > size - pos)
            space = size - pos;
        ret = libxenvchan_write(xenvchan, (char*)data + pos, space);
        if (ret < 0)
            return -1;
        pos += ret;
    }
    return pos;
}

int libvchan_send(libvchan_t *ctrl, const void *data, size_t size) {
    struct libxenvchan *xenvchan = ctrl->xenvchan;

    if (!xenvchan) {
        errno = EAGAIN;
        return -1;
    }
    if (xenvchan->blocking && ctrl->death_xs &&
            size <= (1U << xenvchan->write.order)) {
        while (libxenvchan_is_open(xenvchan) &&
               (size_t)libxenvchan_buffer_space(xenvchan) < size) {
            if (libvchan__wait_event(ctrl, false) &&
                    (size_t)libxenvchan_buffer_space(xenvchan) < size)
                return -1;
        }
    }
    return libxenvchan_send(xenvchan, (char*)data, size);
}

int libvchan_read(libvchan_t *ctrl, void *data, size_t size) {
    struct libxenvchan *xenvchan = ctrl->xenvchan;

    if (!xenvchan) {
        errno = EAGAIN;
        return -1;
    }
    if (xenvchan->blocking && ctrl->death_xs) {
        while (libxenvchan_data_ready(xenvchan) == 0 &&
               libxenvchan_is_open(xenvchan)) {
            if (libvchan__wait_event(ctrl, false) &&
                    libxenvchan_data_ready(xenvchan) == 0)
                return -1;
        }
    }
    return libxenvchan_read(xenvchan, (char*)data, size);
}

void libvchan_set_blocking(libvchan_t *ctrl, bool blocking) {
    if (!ctrl->xenvchan)
        /* this API call doesn't have way to report errors */
        abort();
    ctrl->xenvchan->blocking = blocking;
}

int libvchan_recv(libvchan_t *ctrl, void *data, size_t size) {
    struct libxenvchan *xenvchan = ctrl->xenvchan;

    if (!xenvchan) {
        errno = EAGAIN;
        return -1;
    }
    if (xenvchan->blocking && ctrl->death_xs &&
            size <= (1U << xenvchan->read.order)) {
        while ((size_t)libxenvchan_data_ready(xenvchan) < size &&
               libxenvchan_is_open(xenvchan)) {
            if (libvchan__wait_event(ctrl, false) &&
                    (size_t)libxenvchan_data_ready(xenvchan) < size)
                return -1;
        }
    }
    return libxenvchan_recv(xenvchan, (char*)data, size);
}

int libvchan_wait(libvchan_t *ctrl) {
    int ret = -2; /* invalid, so can be distinguished from real
                     libxenvchan_wait return code */
    struct xs_handle *xs;

    if (!ctrl->xenvchan)
        return 0;

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
         * not hurt. The @releaseDomain watch (privileged callers only) is
         * not used here, see libvchan__arm_death_watch().
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
    ret = libvchan__wait_event(ctrl, true);
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

    if (ctrl->xenvchan)
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
    if (ctrl->death_xs) {
        close(ctrl->epoll_fd);
        xs_close(ctrl->death_xs);
    }
    if (ctrl->xc_handle)
        xc_interface_close(ctrl->xc_handle);
    free(ctrl);
}

EVTCHN libvchan_fd_for_select(libvchan_t *ctrl) {
    if (!ctrl->xenvchan) {
        errno = EAGAIN;
        return -1;
    }
    if (ctrl->death_xs)
        return ctrl->epoll_fd;
    return libxenvchan_fd_for_select(ctrl->xenvchan);
}

int libvchan_data_ready(libvchan_t *ctrl) {
    if (!ctrl->xenvchan)
        return 0;
    return libxenvchan_data_ready(ctrl->xenvchan);
}

int libvchan_buffer_space(libvchan_t *ctrl) {
    if (!ctrl->xenvchan)
        return 0;
    return libxenvchan_buffer_space(ctrl->xenvchan);
}

int libvchan_is_open(libvchan_t *ctrl) {
    int ret;
    struct evtchn_status evst;

    if (!ctrl->xenvchan)
        return VCHAN_WAITING;
    if (ctrl->peer_dead)
        return VCHAN_DISCONNECTED;

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
