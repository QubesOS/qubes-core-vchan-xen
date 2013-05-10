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

#include <stdio.h>
#include <stdlib.h>
#include <xenstore.h>
#include "libvchan.h"

extern xc_interface *xc_handle;

int libvchan_is_open(libvchan_t *ctrl) {
    int ret;
    struct evtchn_status evst;

    ret =  libxenvchan_is_open(ctrl);
    if (!ret)
        return ret;
    /* slow check in case of domain destroy */
    evst.port = ctrl->event_port;
    evst.dom = DOMID_SELF;
    if (xc_evtchn_status(xc_handle, &evst)) {
        perror("xc_evtchn_status");
        return 0;
    }
    if (evst.status != EVTCHNSTAT_interdomain) {
        if (!ctrl->is_server)
            ctrl->ring->srv_live = 0;
        return 0;
    }
    return ret;
}
