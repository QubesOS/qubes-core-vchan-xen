/*
 * The Qubes OS Project, http://www.qubes-os.org
 *
 * Copyright (C) 2013  Marek Marczykowski <marmarek@invisiblethingslab.com>
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

#ifndef LIBVCHAN_PRIVATE_H
#define LIBVCHAN_PRIVATE_H

#include <libxenvchan.h>

struct libvchan {
    struct libxenvchan *xenvchan;
    /*
     * server: path to be removed when client connects,
     * client: path currently watched, for async init
     */
    char *xs_path;
    int remote_domain;
    int local_domain;
    int port;
    xc_interface *xc_handle;
    struct xs_handle *xs;
    /* @releaseDomain watch; NULL if not available */
    struct xs_handle *death_xs;
    /* event channel fd (+ death_xs fd once armed), valid only with death_xs */
    int epoll_fd;
    bool death_watch_armed;
    /* the remote domain died: wait calls fail at once */
    bool peer_dead;
};

int libvchan__check_domain_alive(xc_interface *xc_handle, int dom);
void libvchan__watch_peer_death(libvchan_t *ctrl);

#endif
