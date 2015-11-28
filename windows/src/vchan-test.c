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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <strsafe.h>
#include <xencontrol.h>
#include "libvchan.h"

#if defined(DEBUG) || defined(_DEBUG) || defined(DBG)
#define Log(msg, ...) fprintf(stderr, __FUNCTION__ ": " msg "\n", __VA_ARGS__)
#else
#define Log(msg, ...)
#endif

#define perror(msg) fprintf(stderr, __FUNCTION__ ": " msg " failed: error 0x%x\n", GetLastError())

int libvchan_write_all(libvchan_t *ctrl, char *buf, int size)
{
    int written = 0;
    int ret;

    while (written < size)
    {
        ret = libvchan_write(ctrl, buf + written, size - written);
        if (ret <= 0)
        {
            perror("write");
            exit(1);
        }
        written += ret;
    }
    return size;
}

void write_all(HANDLE fd, char *buf, DWORD size)
{
    DWORD written = 0;
    DWORD tx;

    while (written < size)
    {
        if (!WriteFile(fd, buf + written, size - written, &tx, NULL))
        {
            perror("write");
            exit(1);
        }
        written += tx;
        Log("stdout written %d, total %d", tx, written);
    }
}

void usage()
{
    fprintf(stderr, "usage:\n\tnode server [read|write] domainid nodeid\n"
            "or\n" "\tnode client [read|write] domainid nodeid\n");
    exit(1);
}

#define BUFSIZE 5000
char buf[BUFSIZE];

void reader(libvchan_t *ctrl)
{
    int size;
    HANDLE fd = GetStdHandle(STD_OUTPUT_HANDLE);

    while (1)
    {
        size = rand() % (BUFSIZE - 1) + 1;
        Log("reading %d", size);
        size = libvchan_read(ctrl, buf, size);
        Log("read %d", size);
        fprintf(stderr, "#");

        if (size < 0)
        {
            perror("read vchan");
            libvchan_close(ctrl);
            exit(1);
        }

        if (size == 0)
            break;

        write_all(fd, buf, size);
    }
}

void writer(libvchan_t *ctrl)
{
    int size;
    HANDLE fd = GetStdHandle(STD_INPUT_HANDLE);
    DWORD tx;

    while (1)
    {
        size = rand() % (BUFSIZE - 1) + 1;
        if (!ReadFile(fd, buf, size, &tx, NULL))
        {
            perror("read stdin");
            libvchan_close(ctrl);
            exit(1);
        }

        Log("stdin read %d", tx);

        if (tx == 0)
            break;

        Log("writing %d", tx);
        size = libvchan_write_all(ctrl, buf, tx);
        Log("written %d", size);
        fprintf(stderr, "#");

        if (size < 0)
        {
            perror("vchan write");
            exit(1);
        }

        if (size == 0)
        {
            perror("write size=0?\n");
            exit(1);
        }
    }
}

void XcLogger(int level, const CHAR *function, const WCHAR *format, va_list args)
{
    WCHAR buf[1024];
    StringCbVPrintfW(buf, sizeof(buf), format, args);
    fprintf(stderr, "[X] %s: %S\n", function, buf);
}

/**
    Simple libvchan application, both client and server.
    One side does writing, the other side does reading; both from
    standard input/output fds.
    */
int main(int argc, char **argv)
{
    int seed = (int)time(0);
    libvchan_t *ctrl = 0;
    int wr = 0;

    if (argc < 4)
        usage();

    if (!strcmp(argv[2], "read"))
        wr = 0;
    else if (!strcmp(argv[2], "write"))
        wr = 1;
    else
        usage();

    libvchan_register_logger(XcLogger);

    if (!strcmp(argv[1], "server"))
        ctrl = libvchan_server_init(atoi(argv[3]), atoi(argv[4]), 1024, 1024);
    else if (!strcmp(argv[1], "client"))
        ctrl = libvchan_client_init(atoi(argv[3]), atoi(argv[4]));
    else
        usage();

    if (!ctrl)
    {
        perror("libvchan_*_init");
        exit(1);
    }

    srand(seed);
    fprintf(stderr, "seed=%d\n", seed);

    if (wr)
        writer(ctrl);
    else
        reader(ctrl);
    libvchan_close(ctrl);
    return 0;
}
