# Makefile for Socket
#
# $Id$
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
CFLAGS  = $(SWITCHES) -g $(GCCOPTS)
LDFLAGS = $(SWITCHES) # -s

### You may need to uncomment some lines below for your operating
### system:

### 4.4 BSD (tested on FreeBSD, but probably works for other BSDs)
#SWITCHES = -DHAS_POSIX_SIGS -DHAS_SYS_SIGLIST -DHAS_SYS_ERRLIST

### Linux
#SWITCHES = -DHAS_POSIX_SIGS -DHAS_SYS_SIGLIST -DHAS_SYS_ERRLIST

### Solaris 2.6 and 7 with gcc
SWITCHES = -DHAS_POSIX_SIGS -DHAS_SYS_SIGLIST -DHAS_SYS_ERRLIST \
           -Dsys_siglist=_sys_siglist
SYSLIBS = -lnsl -lsocket


### It should not be necessary to change anything below this line.
##################################################################

MAKE = make
SHELL = /bin/sh
BASE = /home/stone/nickel/src
NODEPATH = socket
NODENAME = "Socket"
TARGET = socket
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
OBJECTS = $(VERSIONOBJECT) socket.o io.o utils.o socketp.o

all: $(TARGET)

$(TARGET): $(LOCALLIBS) $(OBJECTS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJECTS) $(LOCALLIBS) $(SYSLIBS)

busch: 
	rcp -r $(COMPONENTS) busch:src/socket

depend:
	makedepend $(PROGSOURCES) $(VERSIONFILE) 

tags: TAGS
TAGS: $(PROGSOURCES) $(HEADERS)
	etags $(PROGSOURCES) $(HEADERS)

$(VERSIONOBJECT): $(PROGSOURCES)

install: $(INSTALLBINPATH)/$(TARGET) installmanuals

$(INSTALLBINPATH)/$(TARGET): $(TARGET)
	@-echo "installing $(TARGET) in $(INSTALLBINPATH)"; \
	if [ -f $(INSTALLBINPATH)/$(TARGET) ] && \
	   [ ! -w $(INSTALLBINPATH)/$(TARGET) ]; \
	then \
	  chmod u+w $(INSTALLBINPATH)/$(TARGET); \
	fi; \
	cp $(TARGET) $(INSTALLBINPATH)/$(TARGET); \
	chmod $(INSTALLBINMODE) $(INSTALLBINPATH)/$(TARGET); \

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
	rm -f $(TARGET) $(ALIASES) $(OBJECTS) *~ core *.core

$(PROGSOURCES) : $(HEADERS)
