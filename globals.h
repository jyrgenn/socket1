/*
 *   
 * $Id$
 * This file is part of socket(1).
 * Copyright (C) 1992, 1998 by Juergen Nickelsen <jnickelsen@acm.org>
 * Please read the file LICENSE for further details.
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* globals for socket */

#define IN	0		/* standard input */
#define OUT	1		/* standard output */

#define LLEN	100		/* buffer size fo perror2() */

int create_server_socket(int port, int queue_length) ;
int create_client_socket(char **hostname, int port) ;
int resolve_service(char *name_or_number, char *protocol, char **name) ;
/* return host name or IP address string of ip_addr.
 */
char *resolve_ipaddr(struct in_addr *ip_addr) ;
void catchsig(int sig) ;
void usage(void) ;
int do_read_write(int from, int to) ;
int do_write(char *buffer, int size, int to) ;
char *so_release(void) ;
void open_pipes(char *prog) ;
void wait_for_children(void) ;
void perror2(char *s) ;
void add_crs(char *from, char *to, int *sizep) ;
void strip_crs(char *from, char *to, int *sizep) ;
void background(void) ;
void init_sigchld(void) ;
void do_io(void) ;
void initialize_siglist(void) ;
int is_number(char *s) ;


/* global variables */
extern int serverflag ;
extern int loopflag ;
extern int verboseflag ;
extern int readonlyflag ;
extern int Reuseflag ;
extern int writeonlyflag ;
extern int quitflag ;
extern int crlfflag ;
extern int active_socket ;
extern char *progname ;
#ifndef HAS_SYS_ERRLIST
extern char *sys_errlist[] ;
#endif
#ifndef HAS_SYS_SIGLIST
extern char *sys_siglist[] ;
#endif
