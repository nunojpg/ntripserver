#!/bin/bash
# $Id$
# Purpose: Start ntripserver

while true; do ./ntripserver -a www.euref-ip.net -p 80 -m Mountpoint -c password -M 1 -b 19200 -i /dev/gps; sleep 20; done
