/* This file is part of Socket-1.2.
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

#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
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
	"Usage: %s [-bclqrRvvw] [-p prog] [-s | host] port\n" ;

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

/* Set up SIGCHLD handling. */
void init_sigchld(void)
{
#ifdef HAS_POSIX_SIGS
    struct sigaction wait_act ;
    sigset_t sigset ;

    sigfillset(&sigset) ;
    wait_act.sa_handler = wait_act.sa_handler ;
    wait_act.sa_mask = sigset ;
    wait_act.sa_flags = 0 ;
    
#else
#ifdef SIG_SETMASK		/* only with BSD signals */
    static struct sigvec wait_vec = { wait_for_children, ~0, 0 } ;
#endif
#endif

#if !defined (SIGCHLD) && defined (SIGCLD)
#define SIGCHLD SIGCLD
#endif
#ifdef SIGCHLD
#ifdef HAS_POSIX_SIGS
    sigaction(SIGCHLD, &wait_act, 0) ;
#else  /* HAS_POSIX_SIGS */
#ifdef SIG_SETMASK
    sigvec(SIGCHLD, &wait_vec, NULL) ;
#else  /* SIG_SETMASK */
    signal(SIGCHLD, wait_for_children) ;
#endif /* SIG_SETMASK */
#endif /* HAS_POSIX_SIGS */
#endif /* SIGCHLD */
}

/* connect stdin with prog's stdout/stderr and stdout
 * with prog's stdin. */
void open_pipes(char *prog)
{
    int from_cld[2] ;		/* from child process */
    int to_cld[2] ;		/* to child process */

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
void wait_for_children(void)
{
    int wret, status ;
#ifndef ISC
    struct rusage rusage ;
#endif

    /* Just do a wait, forget result */
#ifndef ISC
    while ((wret = wait3(&status, WNOHANG, &rusage)) > 0) ;
#else
    while ((wret = waitpid(-1, &status, WNOHANG)) > 0) ;
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
void strip_crs(from, to, sizep)
char *from, *to ;		/* *from is copied to *to */
int *sizep ;
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
