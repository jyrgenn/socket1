/* This file is part of Socket-1.5.
 */

/*-
 * Copyright (c) 1992 - 2018 Juergen Nickelsen <ni@w21.org>
 * and Boris Nikolaus. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $Id$
 */

#define _BSD                    /* AIX *loves* this */

#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "globals.h"

/* read from from, write to to. select(2) has returned, so input
 * must be available. */
int do_read_write(int from, int to)
{
    int size ;
    char input_buffer[BUFSIZ] ;
    char* buffer;
    char buffer2[2 * BUFSIZ] ;  /* expanding lf's to crlf's can
                                 * make the block twice as big at most */
    int written ;

    if ((size = read(from, input_buffer, BUFSIZ)) == -1) {
        if (errno == EINTR) {
            return 3 ;
        }
        perror2("read") ;
        if (to == active_socket && resetflag) {
            reset_socket_on_close(to) ;
        }
        return 2 ;
    }
    if (size == 0) {            /* end-of-file condition */
        if (from == active_socket) {
            /* if it was the socket, the connection is closed */
            if (verboseflag) {
                fprintf(stderr, "connection closed by peer\n") ;
            }
            if (halfcloseflag && !readonlyflag) {
                close(to) ;
                writeonlyflag = 1 ;
                return -1 ;
            }
            return 0 ;
        } else {
            if (quitflag) {
                /* we close connection later */
                if (verboseflag) {
                    fprintf(stderr, "connection closed\n") ;
                }
                return 0 ;
            }
            if (verboseflag) {
                fprintf(stderr, "end of input on stdin\n") ;
            }
            if (halfcloseflag && !writeonlyflag) {
                shutdown(to, SHUT_WR) ;
                readonlyflag = 1 ;
                return -1 ;
            }
            if (!halfcloseflag) {
                readonlyflag = 1 ;
                return -1 ;
            }
            return 0 ;
        }
    }

    buffer = input_buffer ;
    if (crlfflag) {
        if (to == active_socket) {
            add_crs(buffer, buffer2, &size) ;
        } else {
            strip_crs(buffer, buffer2, &size) ;
        }
        buffer = buffer2 ;
    }
    while (size > 0) {
        written = write(to, buffer, size) ;
        if (written == -1) {
            if (errno == EINTR) {
                return 3 ;
            }
            /* this should not happen */
            perror2("write") ;
            fprintf(stderr, "%s: error writing to %s\n",
                    progname,
                    to == active_socket ? "socket" : "stdout") ;
            if (from == active_socket && resetflag) {
                reset_socket_on_close(from) ;
            }
            return 2 ;
        }
        size -= written ;
        buffer += written ;
    }
    return -1 ;
}

/* all IO to and from the socket is handled here. The main part is
 * a loop around select(2). */
int do_io(void)
{
    fd_set readfds ;
    int fdset_width ;
    int selret ;
    int retval = 0;
    int ioret ;

    fdset_width = (IN > active_socket ? IN : active_socket) + 1 ;
    while (running) {                 /* this loop is exited sideways */
        /* set up file descriptor set for select(2) */
        FD_ZERO(&readfds) ;
        if (!readonlyflag) {
            FD_SET(IN, &readfds) ;
        }
        if (!writeonlyflag) {
            FD_SET(active_socket, &readfds) ;
        }

        do {
            alarm(timeout) ;
            /* wait until input is available */
            selret = select(fdset_width, &readfds, NULL, NULL, NULL) ;
            /* EINTR happens when the process is stopped */
            if (selret < 0 && errno != EINTR) {
                perror2("select") ;
                return 1 ;
            }
            alarm(0) ;
            if (alarmsig_occured) {
                longjmp(setjmp_env, 1) ;
            }
        } while (selret <= 0) ;

        /* do the appropriate read and write */
        if (FD_ISSET(active_socket, &readfds)) {
            ioret = do_read_write(active_socket, OUT) ;
            if (ioret >= 0) {
                retval = ioret ;
                break ;
            }
        }
        if (FD_ISSET(IN, &readfds)) {
            ioret = do_read_write(IN, active_socket) ;
            if (ioret >= 0) {
                retval = ioret ;
                break ;
            }
        }
    }
    return retval ;
}

/* EOF */
