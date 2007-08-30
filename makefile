#!/usr/bin/make
# $Id: makefile,v 1.6 2007/08/30 07:45:59 stoecker Exp $

ntripserver: ntripserver.c
	$(CC) -Wall -W -O3 $? -DNDEBUG -o $@

debug: ntripserver.c
	$(CC) -Wall -W -O3 $? -o ntripserver

clean:
	$(RM) -f ntripserver core

archive:
	tar -cvzf ntripserver.tgz makefile ntripserver.c README startntripserver.sh
