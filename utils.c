/* This file is part of Socket-1.3.
 */

/*-
 * Copyright (c) 1992, 1999, 2000, 2001, 2002, 2003
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
 *	$Id$
 */

#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifdef ISC
#define WNOHANG 1
#else
#include <sys/resource.h>
#endif
#include "globals.h"


/* Give usage message */
void usage(void)
{
    static char ustring[] =
	"Usage: %s [-bclnqrRvvw] [-a bind-address] [-p prog] [-s | host] port\n" ;

    fprintf(stderr, ustring, progname) ;
}

/* perror with progname */
void perror2(char *s)
{
    fprintf(stderr, "%s: ", progname) ;
    perror(s) ;
}

/* is s a number? */
int is_number(char *s)
{
    while (*s) {
	if (*s < '0' || *s > '9') {
	    return 0 ;
	}
	s++ ;
    }
    return 1 ;
}


void handle_sigalrm(int signal)
{
    alarmsig_occured = 1 ;
    fprintf(stderr, "read timeout, closing connection\n") ;
    if (!serverflag || forkflag) {
	exit(0) ;
    }
}


/* Set up signal handling. */
void init_sighandler(int sig, void (*handler)(int))
{
#ifndef HAS_NO_POSIX_SIGS
    struct sigaction handler_act ;
    sigset_t sigset ;

    sigfillset(&sigset) ;
    handler_act.sa_handler = handler ;
    handler_act.sa_mask = sigset ;
    handler_act.sa_flags = 0 ;
    
#else  /* HAS_NO_POSIX_SIGS */
#ifdef SIG_SETMASK		/* only with BSD signals */
    static struct sigvec handler_vec = { handler, ~0, 0 } ;
#endif /* SIG_SETMASK */
#endif /* else HAS_NO_POSIX_SIGS */

#ifndef HAS_NO_POSIX_SIGS
    sigaction(sig, &handler_act, 0) ;
#else  /* HAS_NO_POSIX_SIGS */
#ifdef SIG_SETMASK
    sigvec(sig, &handler_vec, NULL) ;
#else  /* SIG_SETMASK */
    signal(sig, handler) ;
#endif /* SIG_SETMASK */
#endif /* else HAS_NO_POSIX_SIGS */
}

void init_sighandlers(void)
{
    init_sighandler(SIGCHLD, wait_for_children) ;
    init_sighandler(SIGALRM, handle_sigalrm) ;
}


/* Put a variable into the environment. putenv() makes a copy of the
   string on some platforms (namely FreeBSD and Linux), but not on
   others. What a pity. */
void putenv_copy_2(char *varname, char *value)
{
    char *envstr ;

    /* We need space for the strings, a '=', and the null byte. */
    envstr = malloc(strlen(varname) + strlen(value) + 2) ;
    if (envstr == NULL) {
	errno = ENOMEM ;
	perror2("putenv") ;
	return ;		/* simply fail here, that's ok. */
    }
    strcpy(envstr, varname) ;
    strcat(envstr, "=") ;
    strcat(envstr, value) ;
    putenv(envstr) ;
}

/* connect stdin with prog's stdout/stderr and stdout
 * with prog's stdin. */
void open_pipes(char *prog)
{
    int from_cld[2] ;		/* from child process */
    int to_cld[2] ;		/* to child process */
    struct sockaddr_in sa ;
    socklen_t addrlen ;
    char portstr[sizeof("65536")] ; /* enough for 16-bit number */

    /* set environment variables for child process for peer and
     * local address and port */
    addrlen = sizeof(sa) ;
    if (getpeername(active_socket, (struct sockaddr *) &sa, &addrlen) < 0) {
	perror2("getpeername") ; /* should not happen */
	putenv_copy_2(ENVNAME_PEERADDR, "unknown") ;
	putenv_copy_2(ENVNAME_PEERPORT, "unknown") ;
    } else {
	putenv_copy_2(ENVNAME_PEERADDR, resolve_ipaddr(&sa.sin_addr)) ;
	sprintf(portstr, "%d", ntohs(sa.sin_port)) ;
	putenv_copy_2(ENVNAME_PEERPORT, portstr) ;
    }
    addrlen = sizeof(sa) ;
    if (getsockname(active_socket, (struct sockaddr *) &sa, &addrlen) < 0) {
	perror2("getsockname") ; /* should not happen */
	putenv_copy_2(ENVNAME_OWNADDR, "unknown") ;
	putenv_copy_2(ENVNAME_OWNPORT, "unknown") ;
    } else {
	putenv_copy_2(ENVNAME_OWNADDR, resolve_ipaddr(&sa.sin_addr)) ;
	sprintf(portstr, "%d", ntohs(sa.sin_port)) ;
	putenv_copy_2(ENVNAME_OWNPORT, portstr) ;
    }

    /* create pipes */
    if (pipe(from_cld) == -1) {
	perror2("pipe") ;
	exit(errno) ;
    }
    if (pipe(to_cld) == -1) {
	perror2("pipe") ;
	exit(errno) ;
    }

    /* for child process */
    switch (fork()) {
      case 0:			/* this is the child process */
	/* connect stdin to pipe */
	close(0) ;
	close(to_cld[1]) ;
	dup2(to_cld[0], 0) ;
	close(to_cld[0]) ;
	/* connect stdout to pipe */
	close(1) ;
	close(from_cld[0]) ;
	dup2(from_cld[1], 1) ;
	/* connect stderr to pipe */
	close(2) ;
	dup2(from_cld[1], 2) ;
	close(from_cld[1]) ;
	/* call program via sh */
	execl("/bin/sh", "sh", "-c", prog, NULL) ;
	perror2("exec /bin/sh") ;
	/* terminate parent silently */
	kill(getppid(), SIGUSR1) ;
	exit(255) ;
      case -1:
	perror2("fork") ;	/* fork failed */
	exit(errno) ;
      default:			/* parent process */
	/* connect stderr to pipe */
	close(0) ;
	close(from_cld[1]) ;
	dup2(from_cld[0], 0) ;
	close(from_cld[0]) ;
	/* connect stderr to pipe */
	close(1) ;
	close(to_cld[0]) ;
	dup2(to_cld[1], 1) ;
	close(to_cld[1]) ;
    }
}

/* remove zombie child processes */
void wait_for_children(int sig)
{
    int status = 0 ;
#ifndef ISC
    struct rusage rusage ;
#endif

    /* Just do a wait, forget result */
#ifndef ISC
    while (wait3(&status, WNOHANG, &rusage) > 0) ;
#else
    while (waitpid(-1, &status, WNOHANG) > 0) ;
#endif
}

/* expand LF characters to CRLF and adjust *sizep */
void add_crs(char *from, char *to, int *sizep)
{
    int countdown ;		/* counter */

    countdown = *sizep ;
    while (countdown) {
	if (*from == '\n') {
	    *to++ = '\r' ;
	    (*sizep)++ ;
	}
	*to++ = *from++ ;
	countdown-- ;
    }
}

/* strip CR characters from buffer and adjust *sizep */
void strip_crs(char *from, char *to, int *sizep)
{

    int countdown ;		/* counter */

    countdown = *sizep ;
    while (countdown) {
	if (*from == '\r') {
	    from++ ;
	    (*sizep)-- ;
	} else {
	    *to++ = *from++ ;
	}
	countdown-- ;
    }
}

#define NULL_DEVICE "/dev/null"

/* put yourself in the background */
void background(void)
{
    int child_pid ;		/* PID of child process */
    int nulldev_fd ;		/* file descriptor for null device */

    child_pid = fork() ;
    switch (child_pid) {
      case -1:
	perror2("fork") ;
	exit(1) ;
      case 0:
#ifdef NOSETSID
	ioctl(0, TIOCNOTTY, 0) ;
#else
	setsid() ;
#endif
	chdir("/") ;
	if ((nulldev_fd = open(NULL_DEVICE, O_RDWR, 0)) != -1) {
	    int i ;

	    for (i = 0; i < 3; i++) {
		if (isatty(i)) {
		    dup2(nulldev_fd, i) ;
		}
	    }
	    close(nulldev_fd) ;
	}
	break ;
      default:
	exit(0) ;
    }
}

/* EOF */
