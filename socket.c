/*
 *
 * $Id$
 * This file is part of socket(1).
 * Copyright (C) 1992, 1998 by Juergen Nickelsen <jnickelsen@acm.org>
 * Please read the file LICENSE for further details.
 *
 */

#include "globals.h"

#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#ifdef SEQUENT
#include <strings.h>
#else
#include <string.h>
#endif

/* global variables */
int forkflag = 0 ;		/* server forks on connection */
int serverflag = 0 ;		/* create server socket */
int loopflag = 0 ;		/* loop server */
int verboseflag = 0 ;		/* give messages */
int readonlyflag = 0 ;		/* only read from socket */
int writeonlyflag = 0 ;		/* only write to socket */
int quitflag = 0 ;		/* quit connection on EOF */
int crlfflag = 0 ;		/* socket expects and delivers CRLF */
int backgflag = 0 ;		/* put yourself in background */
int active_socket ;		/* socket with connection */
int Reuseflag = 1 ;		/* set server socket SO_REUSEADDR */
char *progname ;		/* name of the game */
char *pipe_program = NULL ;	/* program to execute in two-way pipe */

void server(int port, char *service_name) ;
void handle_server_connection(void) ;
void client(char *host, int port, char *service_name) ;

int main(int argc, char *argv[])
{
    char *cp ;			/* to point to '/' in argv[0] */
    int opt ;			/* option character */
    int error = 0 ;		/* usage error occurred */
    extern int optind ;		/* from getopt() */
    int port ;			/* port number for socket */
    char *service_name ;	/* name of service for port */

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
    while ((opt = getopt(argc, argv, "bcflp:qrRsvw?")) != -1) {
	switch (opt) {
	  case 'f':
	    forkflag = 1 ;
	    break ;
	  case 'c':
	    crlfflag = 1 ;
	    break ;
	  case 'w':
	    writeonlyflag = 1 ;
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
	  default:
	    error++ ;
	}
    }
    if (error ||		/* usage error? */
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
    init_sigchld() ;

    /* get port number */
    port = resolve_service(argv[optind + 1 - serverflag],
			   "tcp", &service_name) ;
    if (port < 0) {
	fprintf(stderr, "%s: unknown service\n", progname) ;
	exit(5) ;
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
    int socket_handle, alen ;

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
	    perror2("accept") ;
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
		    wait_for_children() ;
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
    /* enter IO loop */
    do_io() ;
    /* connection is closed now */
    close(active_socket) ;
    if (pipe_program) {
	/* remove zombies */
	wait_for_children() ;
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
	int addrlen = sizeof(peer) ;

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

/*EOF*/
