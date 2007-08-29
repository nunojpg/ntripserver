#!/bin/make

ntripserver: NtripLinuxServer.c
	$(CC) -Wall -W -O3 $? -DNDEBUG -o $@

debug: NtripLinuxServer.c
	$(CC) -Wall -W -O3 $? -o ntripserver

clean:
	$(RM) -f ntripserver core

archive:
	zip ntripserver.zip -9 NTRIP2.txt makefile NtripLinuxServer.c NtripProvider.doc ReadmeServerLinux.txt SiteLogExample.txt SiteLogInstr.txt StartNtripServerLinux
