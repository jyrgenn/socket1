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
#ifndef NO_INET6
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
#else  /* NO_INET6 */
    struct sockaddr_in sa ;
    int service_result ;
    uint32_t address_result ;
    int s ;
    int one = 1 ;

    memset((char *)&sa, 0, sizeof(sa)) ;
    sa.sin_family = AF_INET ;
    sa.sin_addr.s_addr = INADDR_ANY ;
    sa.sin_port = 0 ;

    service_result = resolve_service(service, "tcp") ;
    if (service_result < 0) {
        fprintf(stderr, "%s: unknown service\n", progname) ;
        exit(5) ;
    }
    sa.sin_port = htons((uint16_t)service_result) ;
    if (bind_address != NULL) {
        address_result = resolve_hostname(bind_address) ;
        if (address_result == INADDR_NONE) {
            fprintf(stderr, "%s: cannot resolve bind address\n", progname) ;
            exit(5) ;
        }
        sa.sin_addr.s_addr = htonl(address_result) ;
    }
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return -1 ;
    }
    if (Reuseflag) {            /* This is default. */
        if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
                       sizeof(one)) < 0) {
            close(s) ;
            return -1 ;
        }
    }
    if (bind(s, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
        close(s) ;
        return -1 ;
    }
    if (listen(s, queue_length) < 0) {
        close(s) ;
        return -1 ;
    }
    if (*addrlen > sizeof(sa)) {
        *addrlen = sizeof(sa) ;
    }
    memcpy(addr, &sa, *addrlen) ;

    return s ;
#endif /* NO_INET6 */
}


/* create a client socket connected to PORT on HOSTNAME */
int create_client_socket(char *bind_address, char *host, char *service,
                         struct sockaddr *addr, socklen_t *addrlen)
{
#ifndef NO_INET6
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
#else /* NO_INET6 */
    struct sockaddr_in sa ;
    int s ;
    uint32_t address_result ;
    int service_result ;

    memset((char *)&sa, 0, sizeof(sa)) ;
    sa.sin_family = AF_INET ;
    sa.sin_addr.s_addr = INADDR_ANY ;
    sa.sin_port = 0 ;

    if (bind_address != NULL) {
        address_result = resolve_hostname(bind_address) ;
        if (address_result == INADDR_NONE) {
            fprintf(stderr, "%s: cannot resolve bind address\n", progname) ;
            exit(5) ;
        }
        sa.sin_addr.s_addr = htonl(address_result) ;
    }
    if ((s = socket(sa.sin_family, SOCK_STREAM, 0)) < 0) { /* get socket */
        return -1 ;
    }
    if (bind(s, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
        close(s) ;
        return -1 ;
    }
    memset((char *)&sa, 0, sizeof(sa)) ;
    sa.sin_family = AF_INET ;
    sa.sin_addr.s_addr = INADDR_ANY ;
    sa.sin_port = 0 ;
    address_result = resolve_hostname(host) ;
    if (address_result == INADDR_NONE) {
        fprintf(stderr, "%s: cannot resolve address\n", progname) ;
        exit(5) ;
    }
    sa.sin_addr.s_addr = ntohl(address_result) ;
    service_result = resolve_service(service, "tcp") ;
    if (service_result < 0) {
        fprintf(stderr, "%s: unknown service\n", progname) ;
        exit(5) ;
    }
    sa.sin_port = htons((uint16_t) service_result) ;
    if (verboseflag >= 2) {
        fprintf(stderr, "trying to connect to %s ... ",
                get_ipaddr(&sa, sizeof(sa))) ;
    }
    alarm(timeout) ;
    if (connect(s, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
        if (verboseflag >= 2) {
            fprintf(stderr, "failed\n") ;
        }
        alarm(0) ;
        close(s) ;
        return -1 ;
    }
    if (verboseflag >= 2) {
        fprintf(stderr, "succeeded\n") ;
    }
    alarm(0) ;
    if (*addrlen > sizeof(sa)) {
        *addrlen = sizeof(sa) ;
    }
    memcpy(addr, &sa, *addrlen) ;
    return s ;
#endif /* NO_INET6 */
}

void reset_socket_on_close(int sd)
{
    struct linger linger ;

    linger.l_onoff = 1 ;
    linger.l_linger = 0 ;
    setsockopt(sd, SOL_SOCKET, SO_LINGER, (char*)&linger, sizeof(linger)) ;
}

#ifndef NO_INET6
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
#else  /* NO_INET6 */
/* resolve IP address for string (decimal dotted or hostname)
 * return address or INADDR_NONE if invalid
 */
uint32_t resolve_hostname(char *host)
{
    uint32_t ret_addr = INADDR_NONE ;
    uint32_t addr ;
    struct hostent *he ;

    if ((addr = inet_addr(host)) != INADDR_NONE) {
        /* is Internet addr in octet notation */
        ret_addr = addr ;
    } else {
        /* do we know the host's address? */
        if (verboseflag >= 2) {
            fprintf(stderr, "resolving %s ... ", host) ;
        }
        if ((he = gethostbyname(host)) != NULL) {
            memcpy(&ret_addr, he->h_addr, sizeof(uint32_t)) ;
            if (verboseflag >= 2) {
                fprintf(stderr, "%s\n",
                        inet_ntoa(*(struct in_addr *)he->h_addr)) ;
            }
        } else {
            if (verboseflag >= 2) {
                fprintf(stderr, "failed\n") ;
            }
            return INADDR_NONE ;
        }
    }
    return ntohl(ret_addr) ;
}

/* return the port number for service NAME_OR_NUMBER.
 */
int resolve_service(char *name_or_number, char *protocol)
{
    struct servent *se ;

    if (is_number(name_or_number)) {
        return atoi(name_or_number) ;
    } else {
        se = getservbyname(name_or_number, protocol) ;
        if (se == NULL) {
            return -1 ;
        }
        return ntohs(se->s_port) ;
    }
}
#endif /* NO_INET6 */

/* return IP address string of a sockaddr.
 */
char *get_ipaddr(struct sockaddr *sa, socklen_t salen)
{
#ifndef NO_INET6
    static char buffer[256] ;

    if (getnameinfo(sa, salen, buffer, sizeof(buffer), NULL, 0,
                    NI_NUMERICHOST) == 0) {
        return buffer ;
    }
    return "unknown" ;
#else  /* NO_INET6 */
    struct in_addr addr ;

    addr = ((struct sockaddr_in *) sa)->sin_addr ;
    return inet_ntoa(addr) ;
#endif /* NO_INET6 */
}

/* return host name of a sockaddr, NULL if no hostname associated with the
 * address or if reverse lookup is disabled
 */
char *get_hostname(struct sockaddr *sa, socklen_t salen)
{
#ifndef NO_INET6
    static char buffer[256] ;

    if (noreverseflag)
        return NULL ;
    if (getnameinfo(sa, salen, buffer, sizeof(buffer), NULL, 0,
                    NI_NAMEREQD) == 0) {
        return buffer ;
    }
    return NULL ;
#else  /* NO_INET6 */
    struct hostent *he ;
    struct in_addr addr ;

    if (noreverseflag)
        return NULL ;
    addr = ((struct sockaddr_in *) sa)->sin_addr ;
    he = gethostbyaddr((const char *) &addr, sizeof(addr), AF_INET) ;
    if (he != NULL) {
        return he->h_name ;
    }
    return NULL ;
#endif /* NO_INET6 */
}

/* return port number string of a sockaddr.
 */
char *get_port(struct sockaddr *sa, socklen_t salen)
{
#ifndef NO_INET6
    static char buffer[16] ;

    if (getnameinfo(sa, salen, NULL, 0, buffer, sizeof(buffer),
                    NI_NUMERICSERV) == 0) {
        return buffer ;
    }
    return "unknown" ;
#else  /* NO_INET6 */
    static char buffer[16] ;

    snprintf(buffer, sizeof(buffer), "%u",
             ntohs(((struct sockaddr_in *) sa)->sin_port)) ;
    return buffer ;
#endif /* NO_INET6 */
}

/* return service name of a sockaddr, NULL if no service name is associated
 * with the port.
 */
char *get_service(struct sockaddr *sa, socklen_t salen, char *protocol)
{
#ifndef NO_INET6
    static char buffer[16] ;

    if (getnameinfo(sa, salen, NULL, 0, buffer, sizeof(buffer), 0) == 0 &&
        !is_number(buffer)) {
        return buffer ;
    }
    return NULL ;
#else  /* NO_INET6 */
    struct servent *se ;

    if ((se = getservbyport(((struct sockaddr_in *) sa)->sin_port,
                            protocol)) != NULL) {
        return se->s_name ;
    }
    return NULL ;
#endif /* NO_INET6 */
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
