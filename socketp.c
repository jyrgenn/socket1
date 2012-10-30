/* This file is part of Socket-1.5.
 */

/*-
 * Copyright (c) 1992, 1999, 2000, 2001, 2002, 2003, 2005
 * Juergen Nickelsen <ni@jnickelsen.de> and Boris Nikolaus. All rights reserved.
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
int create_server_socket(char *bind_address, char *service, int queue_length,
                         struct sockaddr *addr, socklen_t *addrlen)
{
    struct addrinfo hints ;
    struct addrinfo *res ;
    struct addrinfo *ressave ;
    int n ;
    int s ;
    int one = 1 ;

    memset(&hints, 0, sizeof(hints)) ;
    hints.ai_flags = AI_PASSIVE ;
    hints.ai_family = protocol_family ;
    hints.ai_socktype = SOCK_STREAM ;
    if ((n = resolve_hostname_and_service(bind_address, service, &hints, &res))
        != 0) {
        fprintf(stderr, "%s: failed to resolve bind address or service: %s\n",
                progname, gai_strerror(n)) ;
        exit(5) ;
    }
    ressave = res ;
    do {
        if ((s = socket(res->ai_family, res->ai_socktype,
                        res->ai_protocol)) < 0) {
            continue ;
        }
        if (Reuseflag) {            /* This is default. */
            if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
                           sizeof(one)) < 0) {
                close(s) ;
                continue ;
            }
        }
        if (bind(s, res->ai_addr, res->ai_addrlen) < 0) {
            close(s) ;
            continue ;
        }
        if (listen(s, queue_length) < 0) {
            close(s) ;
            continue ;
        }
        if (*addrlen > res->ai_addrlen) {
            *addrlen = res->ai_addrlen ;
        }
        memcpy(addr, res->ai_addr, *addrlen) ;
        break ;
    } while ((res = res->ai_next) != NULL) ;
    if (res == NULL) {
        perror2("failed to create listen socket") ;
        s = -1 ;
    }
    freeaddrinfo(ressave) ;
    return s ;
}


/* create a client socket connected to PORT on HOSTNAME */
int create_client_socket(char *bind_address, char *host, char *service,
                         struct sockaddr *addr, socklen_t *addrlen)
{
    struct addrinfo hints ;
    struct addrinfo *res ;
    struct addrinfo *ressave ;
    struct addrinfo *bindres ;
    int n ;
    int s ;

    memset(&hints, 0, sizeof(hints)) ;
    hints.ai_family = protocol_family ;
    hints.ai_socktype = SOCK_STREAM ;
    if ((n = resolve_hostname_and_service(host, service, &hints, &res)) != 0) {
        fprintf(stderr, "%s: failed to resolve address or service: %s\n",
                progname, gai_strerror(n)) ;
        exit(5) ;
    }
    ressave = res ;
    do {
        if ((s = socket(res->ai_family, res->ai_socktype,
                        res->ai_protocol)) < 0) {
            continue ;
        }
        if (bind_address != NULL) {
            memset(&hints, 0, sizeof(hints)) ;
            hints.ai_flags = AI_PASSIVE ;
            hints.ai_family = res->ai_family ;
            hints.ai_socktype = SOCK_STREAM ;
            if ((n = resolve_hostname_and_service(bind_address, NULL, &hints,
                                                  &bindres)) != 0) {
                continue ;
            }
            if (bind(s, bindres->ai_addr, bindres->ai_addrlen) < 0) {
                close(s) ;
                freeaddrinfo(bindres) ;
                continue ;
            }
            freeaddrinfo(bindres) ;
        }
        if (verboseflag >= 2) {
            fprintf(stderr, "trying to connect to %s ... ",
                    get_ipaddr(res->ai_addr, res->ai_addrlen)) ;
        }
        alarm(timeout) ;
        if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
            if (verboseflag >= 2) {
                fprintf(stderr, "failed\n") ;
            }
            alarm(0) ;
            close(s) ;
            continue ;
        }
        if (verboseflag >= 2) {
            fprintf(stderr, "succeeded\n") ;
        }
        alarm(0) ;
        if (*addrlen > res->ai_addrlen) {
            *addrlen = res->ai_addrlen ;
        }
        memcpy(addr, res->ai_addr, *addrlen) ;
        break ;
    } while ((res = res->ai_next) != NULL) ;
    if (res == NULL) {
        perror2("failed to create or connect socket") ;
        s = -1 ;
    }
    freeaddrinfo(ressave) ;
    return s ;
}

/** Make the TCP connection send a RST on hard close.
 */
void reset_socket_on_close(int sd)
{
    struct linger linger ;

    linger.l_onoff = 1 ;
    linger.l_linger = 0 ;
    setsockopt(sd, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger)) ;
}

/* resolve IP address (decimal dotted or hostname) and service (port number or
 * service name)
 * return a list of addresses
 */
int resolve_hostname_and_service(char *host, char *service,
                                 struct addrinfo *hints, struct addrinfo **res)
{
    int n ;
    struct addrinfo *reshelp ;

    hints->ai_flags |= AI_NUMERICHOST ;
    if ((n = getaddrinfo(host, service, hints, res)) != 0) {
        if (verboseflag >= 2) {
            fprintf(stderr, "resolving %s ... ", host) ;
        }
        hints->ai_flags &= ~AI_NUMERICHOST ;
        if ((n = getaddrinfo(host, service, hints, res)) != 0) {
            if (verboseflag >= 2) {
                fprintf(stderr, "failed\n") ;
            }
            return n ;
        }
        if (verboseflag >= 2) {
            reshelp = *res ;
            do {
                fprintf(stderr, "%s",
                        get_ipaddr(reshelp->ai_addr, reshelp->ai_addrlen)) ;
                if (reshelp->ai_next != NULL) {
                    fprintf(stderr, ", ") ;
                }
            } while ((reshelp = reshelp->ai_next) != NULL) ;
            fprintf(stderr, "\n") ;
        }
    }
    return 0 ;
}

/* return IP address string of a sockaddr.
 */
char *get_ipaddr(struct sockaddr *sa, socklen_t salen)
{
    static char buffer[256] ;

    if (getnameinfo(sa, salen, buffer, sizeof(buffer), NULL, 0,
                    NI_NUMERICHOST) == 0) {
        return buffer ;
    }
    return "unknown" ;
}

/* return host name of a sockaddr, NULL if no hostname associated with the
 * address or if reverse lookup is disabled
 */
char *get_hostname(struct sockaddr *sa, socklen_t salen)
{
    static char buffer[256] ;

    if (noreverseflag)
        return NULL ;
    if (getnameinfo(sa, salen, buffer, sizeof(buffer), NULL, 0,
                    NI_NAMEREQD) == 0) {
        return buffer ;
    }
    return NULL ;
}

/* return port number string of a sockaddr.
 */
char *get_port(struct sockaddr *sa, socklen_t salen)
{
    static char buffer[16] ;

    if (getnameinfo(sa, salen, NULL, 0, buffer, sizeof(buffer),
                    NI_NUMERICSERV) == 0) {
        return buffer ;
    }
    return "unknown" ;
}

/* return service name of a sockaddr, NULL if no service name is associated
 * with the port.
 */
char *get_service(struct sockaddr *sa, socklen_t salen, char *protocol)
{
    static char buffer[16] ;

    if (getnameinfo(sa, salen, NULL, 0, buffer, sizeof(buffer), 0) == 0 &&
        !is_number(buffer)) {
        return buffer ;
    }
    return NULL ;
}

/* return dotted notation for an IPv4 address in host byte order
 */
char *dotted_addr(uint32_t addr)
{
    static char dotted[sizeof("xxx.xxx.xxx.xxx")] ;

    sprintf(dotted, "%d.%d.%d.%d",
            (addr >> 24) & 0xff,
            (addr >> 16) & 0xff,
            (addr >>  8) & 0xff,
            addr         & 0xff) ;
    return dotted ;
}

/* EOF */
