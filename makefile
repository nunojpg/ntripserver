#!/usr/bin/make
# $Id: makefile,v 1.8 2008/01/04 15:22:44 stuerze Exp $

ifdef windir
CC   = gcc
OPTS = -Wall -W -DWINDOWSVERSION
LIBS = -lwsock32
else
OPTS = -Wall -W
endif

ntripserver: ntripserver.c
	$(CC) $(OPTS) $? -O3 -DNDEBUG -o $@ $(LIBS)

debug: ntripserver.c
	$(CC) $(OPTS) $? -g -o ntripserver $(LIBS)

clean:
	$(RM) -f ntripserver core

archive:
	tar -cvzf ntripserver.tgz makefile ntripserver.c README startntripserver.sh
