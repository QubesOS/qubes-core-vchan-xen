/*
 * The Qubes OS Project, http://www.qubes-os.org
 *
 * Copyright (C) 2010  Rafal Wojtczuk  <rafal@invisiblethingslab.com>
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
#include <poll.h>
#include <xenstore.h>
#include <xenctrl.h>
#include <assert.h>
#include "libvchan.h"
#include "libvchan_private.h"

static domid_t parse_domid(const char *ptr) {
    char *endptr;
    unsigned long ret;

    if (!ptr)
        return DOMID_INVALID;
    if (ptr[0] == '0')
        return ptr[1] ? DOMID_INVALID : 0;
    if (ptr[0] < '1' || ptr[0] > '9')
        return DOMID_INVALID;
    ret = strtoul(ptr, &endptr, 10);
    return (*endptr || ret >= DOMID_FIRST_RESERVED) ? DOMID_INVALID : (domid_t)ret;
}

libvchan_t *libvchan_server_init(int domain, int port, size_t read_min, size_t write_min) {
    libvchan_t *ctrl;

    ctrl = calloc(sizeof(*ctrl), 1);
    if (!ctrl)
        return NULL;

    if (asprintf(&ctrl->xs_path, "data/vchan/%d/%d", domain, port) < 0)
        goto err_asprintf;
    ctrl->xenvchan = libxenvchan_server_init(NULL, domain, ctrl->xs_path, read_min, write_min);
    if (!ctrl->xenvchan)
        goto err_server_init;
    ctrl->xenvchan->blocking = 1;
    ctrl->remote_domain = domain;
    if (!(ctrl->xc_handle = xc_interface_open(NULL, NULL, 0)))
        goto err_xc_open;
    return ctrl;
err_xc_open:
    libxenvchan_close(ctrl->xenvchan);
err_server_init:
    free(ctrl->xs_path);
err_asprintf:
    free(ctrl);
    return NULL;
}

libvchan_t *libvchan_client_init_async(int domain, int port, int *watch_fd) {
    libvchan_t *ctrl;
    xc_interface *xc_handle;
    struct xs_handle *xs;

    if (domain < 0 || (unsigned)domain >= DOMID_FIRST_RESERVED) {
        fprintf(stderr, "Invalid peer domain ID %d\n", domain);
        return NULL;
    }

    if (port < 0) {
        fprintf(stderr, "Invalid port %d\n", port);
        return NULL;
    }

    xc_handle = xc_interface_open(NULL, NULL, 0);
    if (!xc_handle) {
        /* error already logged by xc_interface_open */
        goto err;
    }

    /* wait for server to appear */
    xs = xs_open(0);
    if (!xs) {
        perror("xs_open");
        goto err_xc;
    }

    if (!xs_watch(xs, "domid", "domid")) {
        fprintf(stderr, "Cannot setup xenstore watch\n");
        goto err_xs;
    }
    if (!xs_watch(xs, "@releaseDomain", "release")) {
        fprintf(stderr, "Cannot setup xenstore watch\n");
        goto err_xs;
    }

    ctrl = calloc(1, sizeof(*ctrl));
    if (!ctrl)
        goto err_xs;
    ctrl->xs = xs;
    ctrl->xc_handle = xc_handle;
    ctrl->remote_domain = domain;
    ctrl->local_domain = DOMID_INVALID;
    ctrl->port = port;

    /*
     * Watch for the path is established only when domid watch gets fired for
     * the first time
     */
    *watch_fd = xs_fileno(xs);

    return ctrl;

err_xs:
    xs_close(xs);
err_xc:
    xc_interface_close(xc_handle);
err:
    return NULL;
}

int libvchan_client_init_async_finish(libvchan_t *ctrl, bool blocking) {
    char xs_path_base[255];
    char *own_domid;
    char **vec;
    unsigned int len;
    char *tmp_str = NULL;

    for (;;) {
        vec = xs_check_watch(ctrl->xs);
        if (vec) {
            if (strcmp(vec[XS_WATCH_TOKEN], "domid") == 0) {
                /* domid have changed */
                if (ctrl->xs_path) {
                    xs_unwatch(ctrl->xs, ctrl->xs_path, ctrl->xs_path);
                    free(ctrl->xs_path);
                    ctrl->xs_path = NULL;
                }
                ctrl->local_domain = DOMID_INVALID;
            }
            free(vec);
        } else if (errno == EAGAIN) {
            break;
        } else if (errno == EINTR) {
            continue;
        } else {
            return -1;
        }
    }

    /* get local domid after watch gets registered, and after each change */
    if (ctrl->local_domain == DOMID_INVALID) {
        own_domid = xs_read(ctrl->xs, 0, "domid", &len);
        if (!own_domid) {
            fprintf(stderr, "Cannot get own domid\n");
            goto err;
        }
        int own_domid_num = parse_domid(own_domid);
        free(own_domid);
        if (own_domid_num == DOMID_INVALID) {
            fprintf(stderr, "Invalid own domid %s\n", own_domid);
            goto err;
        }
        if (own_domid_num == ctrl->remote_domain) {
            fprintf(stderr, "Loopback vchan connection not supported\n");
            goto err;
        }
        ctrl->local_domain = own_domid_num;
    }

    /* construct xenstore path on first iteration and on every domid
     * change detected (save+restore case) */
    if (!ctrl->xs_path) {
        int v;

        v = snprintf(xs_path_base, sizeof(xs_path_base), "/local/domain/%d/data/vchan/%d/%d",
                     ctrl->remote_domain, ctrl->local_domain, ctrl->port);
        if ((unsigned)v >= sizeof(xs_path_base)) {
            goto err;
        }

        /* watch on this key as we might not have access to the whole directory */
        v = asprintf(&ctrl->xs_path, "%.128s/event-channel", xs_path_base);
        if (v < 0) {
            goto err;
        }

        if (!xs_watch(ctrl->xs, ctrl->xs_path, ctrl->xs_path)) {
            fprintf(stderr, "Cannot setup watch on %s\n", ctrl->xs_path);
            goto err;
        }
    }

    tmp_str = xs_read(ctrl->xs, 0, ctrl->xs_path, &len);
    if (tmp_str)
        free(tmp_str);
    else {
        if (!libvchan__check_domain_alive(ctrl->xc_handle, ctrl->remote_domain)) {
            fprintf(stderr, "domain dead\n");
            goto err;
        }
        /* no xenstore entry (yet), wait more */
        return 1;
    }

    if (!len) {
        /* empty entry, wait more */
        return 1;
    }

    /* when got here, wait is over, attempt to connect */
    xs_close(ctrl->xs);
    ctrl->xs = NULL;

    /* cut trailing /event-channel */
    tmp_str = strrchr(ctrl->xs_path, '/');
    assert(tmp_str);
    *tmp_str = '\0';

    ctrl->xenvchan = libxenvchan_client_init(NULL, ctrl->remote_domain, ctrl->xs_path);
    free(ctrl->xs_path);
    ctrl->xs_path = NULL;
    if (!ctrl->xenvchan)
        goto err;
    ctrl->xenvchan->blocking = blocking;
    /* notify server */
    xc_evtchn_notify(ctrl->xenvchan->event, ctrl->xenvchan->event_port);
    return 0;

err:
    return -1;
}

libvchan_t *libvchan_client_init(int domain, int port) {
    libvchan_t *ctrl;
    int watch_fd, connect_ret = 1;
    struct pollfd vchan_wait = {
        .events = POLLIN,
    };

    ctrl = libvchan_client_init_async(domain, port, &watch_fd);
    if (!ctrl)
        return NULL;

    vchan_wait.fd = watch_fd;
    do {
        if (poll(&vchan_wait, 1, -1) == -1 && errno != EINTR) {
            perror("poll");
            libvchan_close(ctrl);
            return NULL;
        }
        if (vchan_wait.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "unexpected watch_fd event: 0x%x\n", vchan_wait.revents);
            libvchan_close(ctrl);
            return NULL;
        }
        if (vchan_wait.revents & POLLIN) {
            connect_ret = libvchan_client_init_async_finish(ctrl, true);
        }
    } while (connect_ret > 0);

    if (connect_ret < 0) {
        /* error, ctrl already cleaned up */
        return NULL;
    }

    assert(connect_ret == 0);
    /* connected */
    return ctrl;
}
