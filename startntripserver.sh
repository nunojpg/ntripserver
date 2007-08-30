#!/bin/bash
# $Id: startntripserver.sh,v 1.1 2007/08/30 07:45:59 stoecker Exp $
# Purpose: Start ntripserver

while true; do ./ntripserver -M 1 -i /dev/ttys0 -b 9600 -O 2 -a www.euref-ip.net -p 2101 -m Mount2 -n serverID -c serverPass; sleep 60; done
