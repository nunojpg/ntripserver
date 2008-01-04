#!/usr/bin/make
# $Id: makefile,v 1.7 2007/08/30 14:57:34 stuerze Exp $

ifdef windir
CC   = gcc
OPTS = -Wall -W -O3 -DWINDOWSVERSION
LIBS = -lwsock32
else
OPTS = -Wall -W -O3
endif

ntripserver: ntripserver.c
	$(CC) $(OPTS) $? -DNDEBUG -o $@ $(LIBS)

debug: ntripserver.c
	$(CC) $(OPTS) $? -o ntripserver $(LIBS)

clean:
	$(RM) -f ntripserver core

archive:
	tar -cvzf ntripserver.tgz makefile ntripserver.c README startntripserver.sh
