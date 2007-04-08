/* This file is part of Socket-1.5.
 */

/*-
 * Copyright (c) 1992, 1999, 2000, 2001, 2002, 2003, 2005, 2006
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


#include "globals.h"

#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#ifdef SEQUENT
#include <strings.h>
#else
#include <string.h>
#endif

/* global variables */
int forkflag = 0 ;              /* server forks on connection */
int serverflag = 0 ;            /* create server socket */
int loopflag = 0 ;              /* loop server */
int nostderrflag = 0 ;          /* don't write stderr to socket */
int verboseflag = 0 ;           /* give messages */
int readonlyflag = 0 ;          /* only read from socket */
int writeonlyflag = 0 ;         /* only write to socket */
int quitflag = 0 ;              /* quit connection on EOF */
int crlfflag = 0 ;              /* socket expects and delivers CRLF */
int backgflag = 0 ;             /* put yourself in background */
int noreverseflag ;             /* don't do reverse lookup of peer addresses */
int active_socket ;             /* socket with connection */
int Reuseflag = 1 ;             /* set server socket SO_REUSEADDR */
int halfcloseflag = 0 ;         /* support half-close of sockets */
int resetflag = 0 ;             /* reset socket on SIGINT or SIGTERM */
int protocol_family = AF_UNSPEC ;/* protocol family to use */
unsigned int timeout = 0 ;      /* timeout in seconds */
char *progname ;                /* name of the game */
char *pipe_program = NULL ;     /* program to execute in two-way pipe */
char *bind_address = NULL ;     /* address to listen on */

jmp_buf setjmp_env ;            /* buffer for return via longjmp() */
int alarmsig_occured = 0 ;      /* non-zero after alarm signal */

int server(char *service) ;
int handle_server_connection(void) ;
int client(char *host, char *service) ;

int main(int argc, char *argv[])
{
    char *cp ;                  /* to point to '/' in argv[0] */
    int opt ;                   /* option character */
    int error = 0 ;             /* usage error occurred */
    extern int optind ;         /* from getopt() */
    char *service ;             /* service (name or port) for socket */
    int ipv4 = 0 ;              /* IPv4 requested */
    int ipv6 = 0 ;              /* IPv6 requested */
    int exitval ;               /* Exit code of the program */

    /* print version ID if requested */
    if (argv[1] && !strcmp(argv[1], "-version")) {
        puts(so_release()) ;
        exit(0) ;
    }

    /* set up progname for later use */
    progname = argv[0] ;
    if ((cp = strrchr(progname, '/'))) {
        progname = cp + 1 ;
    }

    /* parse options */
    while ((opt = getopt(argc, argv, "a:bcefHlnp:qQrRsvwT:46h?")) != -1) {
        switch (opt) {
          case 'n':
            noreverseflag = 1 ;
            break ;
          case 'a':
            bind_address = optarg ;
            break ;
          case 'f':
            forkflag = 1 ;
            break ;
          case 'c':
            crlfflag = 1 ;
            break ;
          case 'w':
            writeonlyflag = 1 ;
            break ;
          case 'e':
            nostderrflag = 1 ;
            break ;
          case 'p':
            pipe_program = argv[optind - 1] ;
            break ;
          case 'q':
            quitflag = 1 ;
            break ;
          case 'r':
            readonlyflag = 1 ;
            break ;
          case 'R':
            Reuseflag = 0 ;
            break ;
          case 's':
            serverflag = 1 ;
            break ;
          case 'v':
            verboseflag += 1 ;
            break ;
          case 'l':
            loopflag = 1 ;
            break ;
          case 'b':
            backgflag = 1 ;
            break ;
          case 'T':
            timeout = atoi(optarg) ;
            break ;
          case 'H':
            halfcloseflag = 1 ;
            break ;
          case 'Q':
            resetflag = 1 ;
            break ;
          case '4':
            ipv4 = 1 ;
            break ;
          case '6':
            ipv6 = 1 ;
            break ;
          default:
            error++ ;
        }
    }
    if (error ||                /* usage error? */
        argc - optind + serverflag != 2) { /* number of args ok? */
        usage() ;
        exit(15) ;
    }

    /* check some option combinations */
#define senseless(s1, s2) \
    fprintf(stderr, "It does not make sense to set %s and %s.\n", (s1), (s2))

    if (writeonlyflag && readonlyflag) {
        senseless("-r", "-w") ;
        exit(15) ;
    }
    if (loopflag && !serverflag) {
        senseless("-l", "not -s") ;
        exit(15) ;
    }
    if (backgflag && !serverflag) {
        senseless("-b", "not -s") ;
        exit(15) ;
    }
    if (forkflag && !serverflag) {
        senseless("-f", "not -s") ;
    }
    if (ipv4 && !ipv6) {
        protocol_family = AF_INET ;
    } else if (ipv6 && !ipv4) {
#ifdef USE_INET6
        protocol_family = AF_INET6 ;
#else
        fprintf(stderr, "%s: IPv6 not available\n", progname) ;
        exit(15) ;
#endif
    } else {
#ifdef USE_INET6
        protocol_family = AF_UNSPEC ;
#else
        protocol_family = AF_INET ;
#endif
    }

    /* set up signal handling */
    init_sighandlers() ;

    /* get service name */
    service = argv[optind + 1 - serverflag] ;

    /* and go */
    if (serverflag) {
        if (backgflag) {
            background() ;
        }
        exitval = server(service) ;
    } else {
        exitval = client(argv[optind], service) ;
    }
    exit(exitval) ;
}


int server(char *service)
{
    int socket_handle ;
    socklen_t sa_len ;
    char *host_name ;
    char *service_name ;
    char sa_buffer[256] ;
    struct sockaddr *sa = (struct sockaddr *) sa_buffer ;
    int retval = 0 ;

    /* allocate server socket */
    sa_len = sizeof(sa_buffer) ;
    if ((socket_handle = create_server_socket(bind_address, service, 1,
                                              sa, &sa_len)) < 0) {
        return 1 ;
    }
    if (verboseflag) {
        fprintf(stderr, "listening on") ;
        if (bind_address != NULL) {
            fprintf(stderr, " interface address %s", get_ipaddr(sa, sa_len)) ;
            if ((host_name = get_hostname(sa, sa_len)) != NULL) {
                fprintf(stderr, " (%s)", host_name) ;
            }
        }
        fprintf(stderr, " port %s", get_port(sa, sa_len)) ;
        if ((service_name = get_service(sa, sa_len, "tcp")) != NULL) {
            fprintf(stderr, " (%s)", service_name) ;
        }
        fprintf(stderr, "\n") ;
    }

    /* server loop */
    for (;;) {

        /* accept a connection */
        sa_len = sizeof(sa_buffer) ;
        if ((active_socket = accept(socket_handle, sa, &sa_len)) < 0) {
            if (errno == ECONNABORTED)
                continue ;
#ifdef EPROTO
            if (errno == EPROTO)
                continue ;
#endif
            if (errno != EINTR) {
                perror2("accept") ;
                retval = 3 ;
            }
        } else {
            /* if verbose, get name of peer and give message */
            if (verboseflag) {
                fprintf(stderr, "connection from %s", get_ipaddr(sa, sa_len)) ;
                if ((host_name = get_hostname(sa, sa_len)) != NULL) {
                    fprintf(stderr, " (%s)", host_name) ;
                }
                fprintf(stderr, " port %s", get_port(sa, sa_len)) ;
                if ((service_name = get_service(sa, sa_len, "tcp")) != NULL) {
                    fprintf(stderr, " (%s)", service_name) ;
                }
                fprintf(stderr, "\n") ;
            }
            if (forkflag) {
                switch (fork()) {
                  case 0:
                    retval = handle_server_connection() ;
                    exit(retval) ;
                  case -1:
                    perror2("fork") ;
                    break ;
                  default:
                    close(active_socket) ;
                    retval = wait_for_children() ;
                }
            } else {
                retval = handle_server_connection() ;
            }
        }
        if (!loopflag) {
            break ;
        }
    }
    return retval ;
}

int handle_server_connection(void)
{
    int retval ;

    /* open pipes to program, if requested */
    if (pipe_program != NULL) {
        open_pipes(pipe_program) ;
    }
    /* enter IO loop after establishing return point */
    if (!setjmp(setjmp_env)) {
        alarmsig_occured = 0 ;
        retval = do_io() ;
    } else {
        retval = 3 ;
    }
    /* connection is closed now */
    close(active_socket) ;
    if (pipe_program != NULL) {
        /* signal EOF to program */
        close(0) ;
        close(1) ;
        /* remove zombies */
        wait_for_children() ;
    }
    return retval ;
}

int client(char *host, char* service)
{
    char *host_name ;
    char *service_name ;
    char sa_buffer[256] ;
    struct sockaddr *sa = (struct sockaddr *) sa_buffer ;
    socklen_t sa_len ;
    int retval ;

    /* get connection */
    sa_len = sizeof(sa_buffer) ;
    if ((active_socket = create_client_socket(bind_address, host, service,
                                         sa, &sa_len)) < 0) {
        return 1 ;
    }
    if (verboseflag) {
        fprintf(stderr, "connected to %s", get_ipaddr(sa, sa_len)) ;
        if ((host_name = get_hostname(sa, sa_len)) != NULL) {
            fprintf(stderr, " (%s)", host_name) ;
        }
        fprintf(stderr, " port %s", get_port(sa, sa_len)) ;
        if ((service_name = get_service(sa, sa_len, "tcp")) != NULL) {
            fprintf(stderr, " (%s)", service_name) ;
        }
        fprintf(stderr, "\n") ;
    }

    /* open pipes to program if requested */
    if (pipe_program != NULL) {
        open_pipes(pipe_program) ;
    }
    /* enter IO loop */
    retval = do_io() ;
    /* connection is closed now */
    close(active_socket) ;
    return retval ;
}

/* EOF */
