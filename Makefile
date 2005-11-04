# Makefile for Socket-1.3.
#
# Copyright (c) 1992, 1999, 2000, 2001, 2002, 2003
# Juergen Nickelsen <ni@jnickelsen.de>. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#	$Id$
#

### adjust these to your taste and your C compiler.
### This is set up to install socket as /usr/local/bin/socket and
### socket.1 in /usr/local/man/man1/socket.1
### Make sure the target directories exist before doing a "make install".

INSTALLBASE = /usr/local
INSTALLBINPATH = $(INSTALLBASE)/bin
INSTALLBINMODE = 755
INSTALLMANPATH = $(INSTALLBASE)/man
INSTALLMANMODE = 444
GCCOPTS = -Wall -Wstrict-prototypes
CC = cc
CFLAGS = $(SWITCHES) -g
LDFLAGS = $(SWITCHES) # -s

### You may need to uncomment some lines below for your operating
### system:

### 4.4 BSD-derived systems (tested on FreeBSD, but probably works
### for other BSDs)
#SWITCHES = $(GCCOPTS)

### Darwin (MacOS X, actually) 
SWITCHES = -DHAS_NO_SOCKLEN_T $(GCCOPTS)

### Linux (Kernel 2.2.13, SuSE 6.2)
#SWITCHES = -D_GNU_SOURCE $(GCCOPTS)

### Solaris 7
#SWITCHES = -Dsys_siglist=_sys_siglist
#SYSLIBS = -lnsl -lsocket

### IRIX 6.5
#SWITCHES = -DHAS_NO_SOCKLEN_T

### AIX 3.2
# SWITCHES = -DHAS_NO_SOCKLEN_T


### It should not be necessary to change anything below this line.
##################################################################

MAKE = make
SHELL = /bin/sh
BASE = /home/stone/nickel/src
NODEPATH = socket
NODENAME = "Socket"
TARGET = socket
ATARGET = $(ARCHDIR)/$(TARGET)
VERSIONFILE = 	so_release.c
VERSIONOBJECT =	so_release.o
PROGSOURCES = socket.c io.c utils.c socketp.c
SOURCES = BLURB README INSTALL CHANGES \
	socket.1 \
	$(PROGSOURCES) $(VERSIONFILE) scripts
HEADERS = globals.h
MANUALS = $(MAN1)
MAN1 = socket.1
COMPONENTS = $(SOURCES) $(HEADERS) $(MANUALS) Makefile Dependencies
ARCHDIR = $(shell march)
OBJECTS = $(ARCHDIR)/$(VERSIONOBJECT) \
          $(ARCHDIR)/socket.o \
          $(ARCHDIR)/io.o \
          $(ARCHDIR)/utils.o \
          $(ARCHDIR)/socketp.o

all: $(ATARGET)

$(ATARGET): $(ARCHDIR) $(LOCALLIBS) $(OBJECTS)
	$(CC) $(LDFLAGS) -o $(ATARGET) $(OBJECTS) $(LOCALLIBS) $(SYSLIBS)

tags: TAGS
TAGS: $(PROGSOURCES) $(HEADERS)
	etags $(PROGSOURCES) $(HEADERS)

$(ARCHDIR)/%.o : %.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(ARCHDIR):
	mkdir $(ARCHDIR)

$(VERSIONOBJECT): $(PROGSOURCES)

install: $(INSTALLBINPATH)/$(TARGET) installmanuals

$(INSTALLBINPATH)/$(TARGET): $(ATARGET)
	@-echo "installing $(ATARGET) in $(INSTALLBINPATH)"; \
	if [ -f $(INSTALLBINPATH)/$(TARGET) ] && \
	   [ ! -w $(INSTALLBINPATH)/$(TARGET) ]; \
	then \
	  chmod u+w $(INSTALLBINPATH)/$(TARGET); \
	fi; \
	cp $(ATARGET) $(INSTALLBINPATH)/$(TARGET); \
	chmod $(INSTALLBINMODE) $(INSTALLBINPATH)/$(TARGET);

installmanuals: $(MANUALS)
	@-_manuals="$(MAN1)"; \
	for i in $$_manuals; \
	do \
	  echo "installing $$i in $(INSTALLMANPATH)/man1"; \
	  if [ -f $(INSTALLMANPATH)/man1/$$i ] && \
	     [ ! -w $(INSTALLMANPATH)/man1/$$i ]; \
	  then \
	    chmod u+w $(INSTALLMANPATH)/man1/$$i; \
	  fi; \
	  cp $$i $(INSTALLMANPATH)/man1/$$i; \
	  chmod $(INSTALLMANMODE) $(INSTALLMANPATH)/man1/$$i; \
	done

clean:
	rm -f $(ATARGET) $(ALIASES) $(OBJECTS) *~ core *.core

$(OBJECTS) : $(HEADERS)

#EOF
