/* This file is part of Socket-1.2p1.
 */

/*-
 * Copyright (c) 1992, 1999 Juergen Nickelsen <jnickelsen@acm.org>
 * All rights reserved.
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
 *	$Id$
 */

#define _BSD			/* AIX *loves* this */

#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#ifdef ISC
#include <sys/bsdtypes.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "globals.h"

/* read from from, write to to. select(2) has returned, so input
 * must be available. */
int do_read_write(int from, int to)
{
    int size ;
    char input_buffer[BUFSIZ] ;
    
    if ((size = read(from, input_buffer, BUFSIZ)) == -1) {
	perror2("read") ;
	return -1 ;
    }
    if (size == 0) {		/* end-of-file condition */
	if (from == active_socket) {
	    /* if it was the socket, the connection is closed */
	    if (verboseflag) {
		fprintf(stderr, "connection closed by peer\n") ;
	    }
	    return -1 ;
	} else {
	    if (quitflag) {
		/* we close connection later */
		if (verboseflag) {
		    fprintf(stderr, "connection closed\n") ;
		}
		return -1 ;
	    } else if (verboseflag) {
		fprintf(stderr, "end of input on stdin\n") ;
	    }
	    readonlyflag = 1 ;
	    return 1 ;
	}
    }
    return do_write(input_buffer, size, to) ;

}

/* write the buffer; in successive pieces, if necessary. */
int do_write(char *buffer, int size, int to)
{
    char buffer2[2 * BUFSIZ] ;	/* expanding lf's to crlf's can
				 * make the block twice as big at most */
    int written ;

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
	    /* this should not happen */
	    perror2("write") ;
	    fprintf(stderr, "%s: error writing to %s\n",
		    progname,
		    to == active_socket ? "socket" : "stdout") ;
	    return -1 ;
	}
	size -= written ;
    }
    return 1 ;
}

/* all IO to and from the socket is handled here. The main part is
 * a loop around select(2). */
void do_io(void)
{
    fd_set readfds ;
    int fdset_width ;
    int selret ;

    fdset_width = (IN > active_socket ? IN : active_socket) + 1 ;
    while (1) {			/* this loop is exited sideways */
	/* set up file descriptor set for select(2) */
	FD_ZERO(&readfds) ;
	if (!readonlyflag) {
	    FD_SET(IN, &readfds) ;
	}
	if (!writeonlyflag) {
	    FD_SET(active_socket, &readfds) ;
	}

	do {
	    /* wait until input is available */
	    selret = select(fdset_width, &readfds, NULL, NULL, NULL) ;
	    /* EINTR happens when the process is stopped */
	    if (selret < 0 && errno != EINTR) {
		perror2("select") ;
		exit(1) ;
	    }
	} while (selret <= 0) ;

	/* do the appropriate read and write */
	if (FD_ISSET(active_socket, &readfds)) {
	    if (do_read_write(active_socket, OUT) < 0) {
		break ;
	    }
	} else {
	    if (do_read_write(IN, active_socket) < 0) {
		break ;
	    }
	}
    }
}

/* EOF */
