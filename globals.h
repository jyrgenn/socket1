/* This file is part of Socket-1.5.
 */

/*-
 * Copyright (c) 1992 - 2018 Juergen Nickelsen <ni@w21.org>
 * and Boris Nikolaus. All rights reserved.
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <setjmp.h>
#ifndef HAS_NO_INTTYPES_H
#include <inttypes.h>
#endif /* HAS_NO_INTTYPES_H */

/* globals for socket */

#define IN      0               /* standard input */
#define OUT     1               /* standard output */

#define LLEN    100             /* buffer size fo perror2() */

/* Names of environment variables for cild process */
#define ENVNAME_OWNADDR  "SOCKET_LOCAL_ADDRESS"
#define ENVNAME_OWNPORT  "SOCKET_LOCAL_PORT"
#define ENVNAME_PEERADDR "SOCKET_PEER_ADDRESS"
#define ENVNAME_PEERPORT "SOCKET_PEER_PORT"

#ifdef MAX
#undef MAX
#endif
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MAX4(a, b, c, d) MAX(MAX(a, b), MAX(c, d))
#define MAX_ENVNAME_LEN MAX4(sizeof(ENVNAME_OWNADDR),  \
                             sizeof(ENVNAME_OWNPORT),  \
                             sizeof(ENVNAME_PEERADDR), \
                             sizeof(ENVNAME_PEERPORT))
#define MAX_HOSTNAME_LEN 255    /* see RFC 1034 */

#ifdef HAS_NO_SOCKLEN_T
typedef int socklen_t ;
#endif /* HAS_NO_SOCKLEN_T */

/* Solaris has not INADDR_NONE */
#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif /* !INADDR_NONE */

int create_server_socket(char *bind_address, char *service, int queue_length,
                         struct sockaddr *addr, socklen_t *addrlen) ;
int create_client_socket(char *bind_address, char *host, char *service,
                         struct sockaddr *addr, socklen_t *addrlen) ;
void reset_socket_on_close(int sd) ;
#ifndef NO_INET6
int resolve_hostname_and_service(char *host, char *service,
                                 struct addrinfo *hints,
                                 struct addrinfo **res) ;
#else  /* NO_INET6 */
uint32_t resolve_hostname(char *address_or_name) ;
int resolve_service(char *name_or_number, char *protocol) ;
#endif /* NO_INET6 */
char *get_ipaddr(struct sockaddr *sa, socklen_t salen) ;
char *get_hostname(struct sockaddr *sa, socklen_t salen) ;
char *get_port(struct sockaddr *sa, socklen_t salen) ;
char *get_service(struct sockaddr *sa, socklen_t salen, char *protocol) ;
void catchsig(int sig) ;
void usage(void) ;
int do_read_write(int from, int to) ;
char *so_release(void) ;
void open_pipes(char *prog) ;
int wait_for_children(void) ;
void perror2(char *s) ;
void add_crs(char *from, char *to, int *sizep) ;
void strip_crs(char *from, char *to, int *sizep) ;
void background(void) ;
void init_sighandlers(void) ;
int do_io(void) ;
void initialize_siglist(void) ;
int is_number(char *s) ;
char *dotted_addr(uint32_t addr) ;

/* global variables */
extern int serverflag ;
extern int loopflag ;
extern int nostderrflag ;
extern int verboseflag ;
extern int readonlyflag ;
extern int Reuseflag ;
extern int writeonlyflag ;
extern int quitflag ;
extern int crlfflag ;
extern int noreverseflag ;
extern int active_socket ;
extern int forkflag ;
extern int halfcloseflag ;
extern int resetflag ;
extern unsigned int timeout ;
extern char *progname ;
extern char *bind_address ;
extern int protocol_family ;
#ifdef HAS_NO_SYS_ERRLIST
extern char *sys_errlist[] ;
#endif /* HAS_NO_SYS_ERRLIST */
#ifdef HAS_NO_SYS_SIGLIST
extern char *sys_siglist[] ;
#endif /* HAS_NO_SYS_SIGLIST */
extern jmp_buf setjmp_env ;
extern int alarmsig_occured ;
extern int running;

extern int ignore_ret;                  /* ignore syscall return value, but
                                         * assign it to keep the compiler from
                                         * moaning */

/* EOF */
