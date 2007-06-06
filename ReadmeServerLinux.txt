----------------------------------------------------------------------
                        Ntrip Server Linux
----------------------------------------------------------------------

(c) German Federal Agency for Cartography and Geodesy (BKG), 2002-2007


Files in NtripServerLinux.zip
-----------------------------
ReadmeServerLinux.txt: Readme file for NtripServerLinux
NtripServerLinux.tar: NtripServerLinux program tar archive
NtripProvider.doc: Server password/mountpoit request form
SiteLogExample.txt: Example Station Logfile
SiteLogInstr.txt: Station Logfile Instructions

Ntrip
-----
NtripServerLinux is a HTTP client based on "Networked Transport of 
RTCM via Internet Protocol" (Ntrip). This is an application-level 
protocol streaming Global Navigation Satellite System (GNSS) data over 
the Internet. Ntrip is a generic, stateless protocol based on the 
Hypertext Transfer Protocol HTTP/1.1. The HTTP objects are enhanced 
to GNSS data streams.

Ntrip is designed for disseminating differential correction data 
(e.g in the RTCM-104 format) or other kinds of GNSS streaming data to
stationary or mobile users over the Internet, allowing simultaneous
PC, Laptop, PDA, or receiver connections to a broadcasting host. Ntrip
supports wireless Internet access through Mobile IP Networks like GSM,
GPRS, EDGE, or UMTS.

Ntrip is implemented in three system software components:
NtripClients, NtripServers and NtripCasters. The NtripCaster is the
actual HTTP server program whereas NtripClient and NtripServer are
acting as HTTP clients.

NtripServerLinux
----------------
The program NtripServerLinux is designed to provide real-time data
from a single NtripSource running under a Linux operating system.
Basically the NtripServerLinux grabs a GNSS byte stream
from a serial port or tcpsocket port and sends it off over an
Internet TCP connection to the NtripCaster.

Mind that the NtripServerLinux may not be able to handle your
proxyserver. When using it in a proxy-protected Local Area Network
(LAN), a TCP-relay may have to be established connecting the
proxyserver and the NtripCaster. Establishing the Internet
connection for an NtripServerLinux by using an Internet Service
Provider (ISP) is an alternative. 

Installation
------------
To install the program 
- unzip file NtripServerLinux.zip
- run tar -xf NtripServerLinux.tar
- change directory to NtripServerLinux
- run make
The exacutable will show up as NtripServerLinux.

Usage
-----
The user may call the program with the following options:

    -a DestinationCaster name or address (default: www.euref-ip.net)
    -p DestinationCaster port (default: 80)
    -m DestinationCaster mountpoint
    -c DestinationCaster password
    -h|? print this help screen
    -M <mode>  sets the input mode
               (1=serial, 2=tcpsocket, 3=file, 4=sisnet, 5=udpsocket, 6=caster)
  Mode = file:
    -s file, simulate data stream by reading log file
       default/current setting is /dev/stdin
  Mode = serial:
    -b baud_rate, sets serial input baud rate
       default/current value is 19200
    -i input_device, sets name of serial input device
       default/current value is /dev/gps
       (normally a symbolic link to /dev/tty??)
  Mode = tcpsocket or udpsocket:
    -P receiver port (default: 1025)
    -H hostname of TCP server (default: 127.0.0.1)
    -f initfile send to server
    -x receiver id
    -y receiver password
    -B bindmode: bind to incoming UDP stream
  Mode = sisnet:
    -P receiver port (default: 7777)
    -H hostname of TCP server (default: 131.176.49.142)
    -u username
    -l password
    -V version [2.1 or 3.1] (default: 2.1)
  Mode = caster:
    -P SourceCaster port (default: 80)
    -H SourceCaster hostname (default: www.euref-ip.net)
    -D SourceCaster mountpoint
    -U SourceCaster username
    -W SourceCaster password

Example:
NtripServerLinux -a www.euref-ip.net -p 2101 -m mountpoint -c password -M 1 -b 19200 -i /dev/ttyS0

It is recommended to start NtripServerLinux through shell script
StartNtripServerLinux. This shell script ensures that
NtripServerLinux reconnects to the NtripCaster after a broken
connection.

NtripCaster IP address
----------------------
The current Internet address of the Ntrip Broadcaster which has to be
introduced in the NtripServerLinux is "www.euref-ip.net". The port
number is "80" or "2101".

Server password and mountpoint
------------------------------
Feeding data streams into the Ntrip system using the
NtripServerLinux program needs a server password and one mountpoint
per stream. Currently this is available from
euref-ip@bkg.bund.de (see "NtripProvider.doc").

Station Logfile
---------------
A user of your data stream may need detailed information about the
GNSS hardware and firmware that generates your signal. This
information will be made available through a station logfile. Please
find an example station logfile in "SiteLogExample.txt". Create a
similar logfile describing your GNSS receiver hardware and firmware
and include the requested information as far as it is available for
you. Note that the form of this document follows an IGS
recommendatation that can be downloaded from
ftp://igscb.jpl.nasa.gov/pub/station/general/sitelog_instr.txt
The content of your station logfile has to be kept up to date.
Thus, please inform the NtripCaster operator about all changes at
your station by sending an updates version of your station logfile.
Providing a station logfile is not necessary in case you generate
a Virtual Reference Station (VRS) data stream.


Disclaimer
----------
Note that this example server implementation is currently an
experimental software. The BKG disclaims any liability nor
responsibility to any person or entity with respect to any loss or
damage caused, or alleged to be caused, directly or indirectly by the
use and application of the Ntrip technology.

Further information
-------------------
http://igs.bkg.bund.de/index_ntrip.htm
euref-ip@bkg.bund.de

