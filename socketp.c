/* This file is part of Socket-1.3.
 */

/*-
 * Copyright (c) 1992, 1999 Juergen Nickelsen <ni@jnickelsen.de>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "globals.h"

/*
 * create a server socket on PORT accepting QUEUE_LENGTH connections
 */
int create_server_socket(int port, int queue_length)
{
    struct sockaddr_in sa ;
    int s ;
    int one = 1 ;

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	return -1 ;
    }
    if (Reuseflag) {		/* This is default. */
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
		       sizeof(one)) < 0) {
	    return -1 ;
	}
    }

    memset((char *) &sa, 0, sizeof(sa)) ;
    sa.sin_family = AF_INET ;
    sa.sin_addr.s_addr = htonl(bind_addr) ;
    sa.sin_port = htons(port) ;

    if (bind(s, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
	return -1 ;
    }
    if (listen(s, queue_length) < 0) {
	return -1 ;
    }

    return s ;
}


/* create a client socket connected to PORT on HOSTNAME */
int create_client_socket(char **hostname, int port)
{
    struct sockaddr_in sa, local_sa ;
    struct hostent *hp ;
    int s ;
    long addr ;


    memset(&sa, 0, sizeof(sa)) ;
    if ((addr = inet_addr(*hostname)) != -1) {
	/* is Internet addr in octet notation */
	memcpy((char *) &sa.sin_addr, &addr, sizeof(addr)) ; /* set address */
	sa.sin_family = AF_INET ;
    } else {
	/* do we know the host's address? */
	if (verboseflag >= 2) {
	    fprintf(stderr, "resolving %s... ", *hostname) ;
	}
	if ((hp = gethostbyname(*hostname)) == NULL) {
	    return -2 ;
	}
	*hostname = hp->h_name ;
	memcpy((char *) &sa.sin_addr, hp->h_addr, hp->h_length) ;
	sa.sin_family = hp->h_addrtype ;
	if (verboseflag >= 2) {
	    fprintf(stderr, "%s\n", inet_ntoa(sa.sin_addr)) ;
	}
    }

    sa.sin_port = htons((u_short) port) ;

    if ((s = socket(sa.sin_family, SOCK_STREAM, 0)) < 0) { /* get socket */
	return -1 ;
    }
    if (bind_addr != INADDR_ANY) {
	memset((char *) &local_sa, 0, sizeof(local_sa)) ;
	local_sa.sin_family = AF_INET ;
	local_sa.sin_addr.s_addr = htonl(bind_addr) ;

	if (bind(s, (struct sockaddr *) &local_sa, sizeof(local_sa)) < 0) {
	    return -1 ;
	}
    }
    if (verboseflag >= 2) {
	fprintf(stderr, "trying... ") ;
    }
    if (connect(s, (struct sockaddr *) &sa, sizeof(sa)) < 0) { /* connect */
	close(s) ;
	return -1 ;
    }
    return s ;
}

/* return the port number for service NAME_OR_NUMBER. If NAME is non-null,
 * the name is the service is written there.
 */
int resolve_service(char *name_or_number, char *protocol, char **name)
{
    struct servent *servent ;
    int port ;

    if (is_number(name_or_number)) {
	port = atoi(name_or_number) ;
	if (name != NULL) {
	    servent = getservbyport(htons(port), protocol) ;
	    if (servent != NULL) {
		*name = servent->s_name ;
	    } else {
		*name = NULL ;
	    }
	}
	return port ;
    } else {
	servent = getservbyname(name_or_number, protocol) ;
	if (servent == NULL) {
	    return -1 ;
	}
	if (name != NULL) {
	    *name = servent->s_name ;
	}
	return ntohs(servent->s_port) ;
    }
}

/* return host name or IP address string of ip_addr.
 */
char *resolve_ipaddr(struct in_addr *ip_addr)
{
    struct hostent *he ;

    if (noreverseflag) {
	return inet_ntoa(*ip_addr) ;
    } else {
	he = gethostbyaddr((const char *) &ip_addr->s_addr,
			   sizeof(ip_addr->s_addr), AF_INET) ;
	return he ? he->h_name : inet_ntoa(*ip_addr) ;
    }
}

char *dotted_addr(unsigned long addr)
{
    static char dotted[sizeof("xxx.xxx.xxx.xxx")] ;

    sprintf(dotted, "%ld.%ld.%ld.%ld",
	    (addr >> 24) & 0xff,
	    (addr >> 16) & 0xff,
	    (addr >>  8) & 0xff,
	    addr         & 0xff) ;
    return dotted ;
}

/* resolve IP address for string (decimal dotted or hostname)
 * return address or INADDR_NONE if invalid
 */
unsigned long resolve_name(char *address_or_name)
{
    unsigned long ret_addr = INADDR_NONE ;
    unsigned long addr ;
    struct hostent *hp ;

    if ((addr = inet_addr(address_or_name)) != INADDR_NONE) {
	/* is Internet addr in octet notation */
	ret_addr = addr ;
    } else {
	/* do we know the host's address? */
	if (verboseflag >= 2) {
	    fprintf(stderr, "resolving %s... ", address_or_name) ;
	}
	if ((hp = gethostbyname(address_or_name)) != NULL) {
	    memcpy(&ret_addr, hp->h_addr, sizeof(unsigned long)) ;
	    if (verboseflag >= 2) {
		fprintf(stderr, "%s\n", dotted_addr(htonl(ret_addr))) ;
	    }
	}
    }
    return htonl(ret_addr) ;
}

/* EOF */
