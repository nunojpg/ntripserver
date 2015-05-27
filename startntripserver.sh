#!/bin/bash
# $Id: startntripserver.sh,v 1.2 2007/08/30 15:02:19 stuerze Exp $
# Purpose: Start ntripserver

while true; do ./ntripserver -M 1 -i /dev/ttys0 -b 9600 -O 2 -a www.euref-ip.net -p 2101 -m Mount2 -n serverID -c serverPass; sleep 60; done
