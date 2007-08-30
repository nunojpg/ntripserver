#!/usr/bin/make
# $Id$

ntripserver: ntripserver.c
	$(CC) -Wall -W -O3 $? -DNDEBUG -o $@

debug: ntripserver.c
	$(CC) -Wall -W -O3 $? -o ntripserver

clean:
	$(RM) -f ntripserver core

archive:
	tar -xzf ntripserver.tgz -9 makefile ntripserver.c NtripProvider.doc \
        README SiteLogExample.txt SiteLogInstr.txt startntripserver.sh
