/* This file is part of Socket-1.4.
 */

/*-
 * Copyright (c) 1992, 1999, 2000, 2001, 2002, 2003, 2005
 * Juergen Nickelsen <ni@jnickelsen.de>. All rights reserved.
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
unsigned int timeout = 0 ;      /* timeout in seconds */
char *progname ;                /* name of the game */
char *pipe_program = NULL ;     /* program to execute in two-way pipe */
uint32_t bind_addr = INADDR_ANY;/* address to listen on */

jmp_buf setjmp_env ;            /* buffer for return via longjmp() */
int alarmsig_occured = 0 ;      /* non-zero after alarm signal */

void server(int port, char *service_name) ;
void handle_server_connection(void) ;
void client(char *host, int port, char *service_name) ;

int main(int argc, char *argv[])
{
    char *cp ;                  /* to point to '/' in argv[0] */
    int opt ;                   /* option character */
    int error = 0 ;             /* usage error occurred */
    extern int optind ;         /* from getopt() */
    int port ;                  /* port number for socket */
    char *service_name ;        /* name of service for port */
    char *bind_name = 0 ;       /* name supplied to -a option */

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
    while ((opt = getopt(argc, argv, "a:bceflnp:qrRsvwT:?")) != -1) {
        switch (opt) {
          case 'n':
            noreverseflag = 1 ;
            break ;
          case 'a':
            bind_name = optarg ;
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
 
    /* set up signal handling */
    init_sighandlers() ;

    /* get port number */
    port = resolve_service(argv[optind + 1 - serverflag],
                           "tcp", &service_name) ;
    if (port < 0) {
        fprintf(stderr, "%s: unknown service\n", progname) ;
        exit(5) ;
    }

    if (bind_name) {
        bind_addr = resolve_name(bind_name) ;
        if (bind_addr == INADDR_NONE) {
            perror2("can't resolve bind address") ;
            exit(2) ;
        }
    }

    /* and go */
    if (serverflag) {
        if (backgflag) {
            background() ;
        }
        server(port, service_name) ;
    } else {
        client(argv[optind], port, service_name) ;
    }          
    exit(0) ;
}


void server(int port, char *service_name)
{
    int socket_handle ;
    socklen_t alen ;

    /* allocate server socket */
    socket_handle = create_server_socket(port, 1) ;
    if (socket_handle < 0) {
        perror2("server socket") ;
        exit(1) ;
    }
    if (verboseflag) {
        fprintf(stderr, "listening on port %d", port) ;
        if (service_name) {
            fprintf(stderr, " (%s)", service_name) ;
        }
        fprintf(stderr, "\n") ;
    }

    /* server loop */
    do {
        struct sockaddr_in sa ;

        alen = sizeof(sa) ;

        /* accept a connection */
        if ((active_socket = accept(socket_handle,
                          (struct sockaddr *) &sa,
                          &alen)) == -1) {
            if (errno != EINTR) {
                perror2("accept") ;
            }
        } else {
            /* if verbose, get name of peer and give message */
            if (verboseflag) {
                fprintf(stderr, "connection from %s\n",
                        resolve_ipaddr(&sa.sin_addr)) ;
            }
            if (forkflag) {
                switch (fork()) {
                  case 0:
                    handle_server_connection() ;
                    exit(0) ;
                  case -1:
                    perror2("fork") ;
                    break ;
                  default:
                    close(active_socket) ;
                    wait_for_children(0) ;
                }
            } else {
                handle_server_connection() ;
            }
        }
    } while (loopflag) ;
}


void handle_server_connection(void)
{
    /* open pipes to program, if requested */
    if (pipe_program != NULL) {
        open_pipes(pipe_program) ;
    }
    /* enter IO loop after establishing return point */
    if (!setjmp(setjmp_env)) {
        alarmsig_occured = 0 ;
        do_io() ;
    }
    /* connection is closed now */
    close(active_socket) ;
    if (pipe_program) {
        /* remove zombies */
        wait_for_children(0) ;
    }
}


void client(char *host, int port, char *service_name)
{
    /* get connection */
    active_socket = create_client_socket(&host, port) ;
    if (active_socket == -1) {
        perror2("client socket") ;
        exit(errno) ;
    } else if (active_socket == -2) {
        fprintf(stderr, "%s: unknown host %s\n", progname, host) ;
        exit(13) ;
    }
    if (verboseflag) {
        struct sockaddr_in peer ;
        socklen_t addrlen = sizeof(peer) ;

        if (getpeername(active_socket,
                        (struct sockaddr *) &peer, &addrlen) < 0) {
            perror2("getpeername") ;
        } else {
            host = resolve_ipaddr(&peer.sin_addr) ;
            port = ntohs(peer.sin_port) ;
        }
        fprintf(stderr, "connected to %s (%s) port %d",
                inet_ntoa(peer.sin_addr), host, port) ;
        if (service_name) {
            fprintf(stderr, " (%s)", service_name) ;
        }
        fprintf(stderr, "\n") ;
    }

    /* open pipes to program if requested */
    if (pipe_program != NULL) {
        open_pipes(pipe_program) ;
    }
    /* enter IO loop */
    do_io() ;
    /* connection is closed now */
    close(active_socket) ;
}

/* EOF */
