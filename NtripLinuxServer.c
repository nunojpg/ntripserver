/*
 * NtripServerLinux.c
 *
 * Copyright (c) 2003...2005
 * German Federal Agency for Cartography and Geodesy (BKG)
 *
 * Developed for Networked Transport of RTCM via Internet Protocol (NTRIP)
 * for streaming GNSS data over the Internet.
 *
 * Designed by Informatik Centrum Dortmund http://www.icd.de
 *
 * NTRIP is currently an experimental technology.
 * The BKG disclaims any liability nor responsibility to any person or
 * entity with respect to any loss or damage caused, or alleged to be 
 * caused, directly or indirectly by the use and application of the NTRIP 
 * technology.
 *
 * For latest information and updates, access:
 * http://igs.ifag.de/index_ntrip.htm
 *
 * Georg Weber
 * BKG, Frankfurt, Germany, June 2003-06-13
 * E-mail: euref-ip@bkg.bund.de
 *
 * Based on the GNU General Public License published nmead
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

/* $Id: NtripLinuxServer.c,v 1.10 2005/04/27 08:32:43 stoecker Exp $
 * Changes - Version 0.7
 * Sep 22 2003  Steffen Tschirpke <St.Tschirpke@actina.de>
 *           - socket support
 *           - command line option handling
 *           - error handling
 *           - help screen
 *
 * Changes - Version 0.9
 * Feb 15 2005  Dirk Stoecker <soft@dstoecker.de>
 *           - some minor updates, fixed serial baudrate settings
 *
 * Changes - Version 0.10
 * Apr 05 2005  Dirk Stoecker <soft@dstoecker.de>
 *           - some cleanup and miscellaneous fixes
 *           - replaced non-working simulate with file input (stdin)
 *           - TCP sending now somewhat more stable
 *           - cleanup of error handling
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/termios.h>
#include <sys/types.h>

#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0 /* prevent compiler errors */
#endif
#ifndef O_EXLOCK
#define O_EXLOCK 0 /* prevent compiler errors */
#endif

enum MODE { SERIAL = 1, TCPSOCKET = 2, INFILE = 3 };

#define VERSION         "NTRIP NtripServerLinux/0.10"
#define BUFSZ           1024

/* default socket source */
#define SERV_HOST_ADDR  "127.0.0.1"
#define SERV_TCP_PORT   1025

/* default destination */
#define NTRIP_CASTER    "www.euref-ip.net"
#define NTRIP_PORT      80

static int ttybaud             = 19200;
static const char *ttyport     = "/dev/gps";
static const char *filepath    = "/dev/stdin";
static enum MODE mode          = INFILE;

/* Forward references */
static int openserial(const char * tty, int blocksz, int baud);
static void send_receive_loop(int sock, int fd);
static void usage(int);

/*
* main
*
* Main entry point for the program.  Processes command-line arguments and
* prepares for action.
*
* Parameters:
*     argc : integer        : Number of command-line arguments.
*     argv : array of char  : Command-line arguments as an array of zero-terminated
*                             pointers to strings.
*
* Return Value:
*     The function does not return a value (although its return type is int).
*
* Remarks:
*
*/

int main(int argc, char **argv)
{
  int c, gpsfd = -1;
  int size = 2048;              /* for setting send buffer size */

  unsigned int out_port = 0;
  unsigned int in_port = 0;
  const char *mountpoint = NULL;
  const char *password = "";
  const char *initfile = NULL;
  int sock_id;
  char szSendBuffer[BUFSZ];
  int nBufferBytes;

  struct hostent *inhost;
  struct hostent *outhost;

  struct sockaddr_in in_addr;
  struct sockaddr_in out_addr;

  if(!(outhost = gethostbyname(NTRIP_CASTER)))
  {
    fprintf(stderr, "WARNING: host %s unknown\n", NTRIP_CASTER);
  }
  else
  {
    memset((char *) &out_addr, 0x00, sizeof(out_addr));
    memcpy(&out_addr.sin_addr, outhost->h_addr, (size_t)outhost->h_length);
  }

  /* get and check program arguments */
  if(argc <= 1)
  {
    usage(2);
    exit(1);
  }
  while((c = getopt(argc, argv, "M:i:h:b:p:s:a:m:c:H:P:f:")) != EOF)
  {
    switch (c)
    {
    case 'M':
      if(!strcmp(optarg, "serial")) mode = 1;
      else if(!strcmp(optarg, "tcpsocket")) mode = 2;
      else if(!strcmp(optarg, "file")) mode = 3;
      else mode = atoi(optarg);
      if((mode == 0) || (mode > 3))
      {
        fprintf(stderr, "ERROR: can't convert %s to a valid mode\n", optarg);
        usage(-1);
      }
      break;
    case 'i':                  /* gps serial ttyport */
      ttyport = optarg;
      break;
    case 'b':                  /* serial ttyin speed */
      ttybaud = atoi(optarg);
      if(ttybaud <= 1)
      {
        fprintf(stderr, "ERROR: can't convert %s to valid serial speed\n", optarg);
        usage(1);
      }
      break;
    case 'a':                  /* http server IP address A.B.C.D */
      outhost = gethostbyname(optarg);
      if(outhost == NULL)
      {
        fprintf(stderr, "ERROR: host %s unknown\n", optarg);
        usage(-2);
      }
      memset((char *) &out_addr, 0x00, sizeof(out_addr));
      memcpy(&out_addr.sin_addr, outhost->h_addr, (size_t)outhost->h_length);
      break;
    case 'p':                  /* http server port */
      out_port = atoi(optarg);
      if(out_port <= 1)
      {
        fprintf(stderr, "ERROR: can't convert %s to a valid HTTP server port\n",
          optarg);
        usage(1);
      }
      break;
    case 'm':                  /* http server mountpoint */
      mountpoint = optarg;
      break;
    case 's':                  /* datastream from file */
      filepath = optarg;
      break;
    case 'f':
      initfile = optarg;
      break;
    case 'c':                  /* password */
      password = optarg;
      break;
    case 'H':                  /* host */
      inhost = gethostbyname(optarg);
      if(inhost == NULL)
      {
        fprintf(stderr, "ERROR: host %s unknown\n", optarg);
        usage(-2);
      }
      memset((char *) &in_addr, 0x00, sizeof(in_addr));
      memcpy(&in_addr.sin_addr, inhost->h_addr, (size_t)inhost->h_length);
      break;
    case 'P':                  /* port */
      in_port = atoi(optarg);
      if(in_port <= 1)
      {
        fprintf(stderr, "ERROR: can't convert %s to a valid receiver port\n",
          optarg);
        usage(1);
      }
      break;
    case 'h':                  /* help */
    case '?':
      usage(0);
      break;
    default:
      usage(2);
      break;
    }

    if(in_port <= 0)
    {
      in_port = SERV_TCP_PORT;
    }
    if(out_port <= 0)
    {
      out_port = NTRIP_PORT;
    }
  }

  argc -= optind;
  argv += optind;

  if(argc > 0)
  {
    fprintf(stderr, "ERROR: Extra args on command line: ");
    for(; argc > 0; argc--)
    {
      fprintf(stderr, " %s", *argv++);
    }
    fprintf(stderr, "\n");
    usage(1);                   /* never returns */
  }

  if(mountpoint == NULL)
  {
    fprintf(stderr, "ERROR: Missing mountpoint argument\n");
    exit(1);
  }
  if(!password[0])
  {
    fprintf(stderr, "WARNING: Missing password argument - are you really sure?\n");
  }

  switch (mode)
  {
  case INFILE:
    {
      gpsfd = open(filepath, O_RDONLY);
      if(!gpsfd)
      {
        perror("ERROR: opening input file");
        exit(1);
      }
      /* set blocking mode in case it was not set (seems to be sometimes for fifo's) */
      fcntl(gpsfd, F_SETFL, 0);
      printf("file input: file = %s\n", filepath);
    }
    break;
  case SERIAL:                 /* open serial port */
    {
      gpsfd = openserial(ttyport, 1, ttybaud);
      if(gpsfd < 0)
      {
        exit(1);
      }
      printf("serial input: device = %s, speed = %d\n", ttyport, ttybaud);
    }
    break;
  case TCPSOCKET:
    {
      in_addr.sin_family = AF_INET;
      in_addr.sin_port = htons(in_port);

      if((gpsfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      {
        fprintf(stderr, "ERROR: can't create socket\n");
        exit(1);
      }

      printf("socket input: host = %s, port = %d%s%s\n",
        inet_ntoa(in_addr.sin_addr), in_port, initfile ? ", initfile = " : "",
        initfile ? initfile : "");

      if(connect(gpsfd, (struct sockaddr *) &in_addr, sizeof(in_addr)) < 0)
      {
        fprintf(stderr, "ERROR: can't connect input to %s at port %d\n",
          inet_ntoa(in_addr.sin_addr), in_port);
        exit(1);
      }
      if(initfile)
      {
        char buffer[1024];
        FILE *fh;
        int i;

        if((fh = fopen(initfile, "r")))
        {
          while((i = fread(buffer, 1, sizeof(buffer), fh)) > 0)
          {
            if((send(gpsfd, buffer, (size_t)i, 0)) != i)
            {
              perror("ERROR: sending init file");
              exit(1);
            }
          }
          if(i < 0)
          {
            perror("ERROR: reading init file");
            exit(1);
          }
          fclose(fh);
        }
        else
        {
          fprintf(stderr, "ERROR: can't read init file %s\n", initfile);
          exit(1);
        }
      }
    }
    break;
  default:
    usage(-1);
    break;
  }

  out_addr.sin_family = AF_INET;
  out_addr.sin_port = htons((u_short) (out_port));

  /* ----- main part ----- */
  while(1)
  {
    /* create socket */
    if((sock_id = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      fprintf(stderr, "ERROR : could not create socket\n");
      exit(2);
    }
    /* connect to caster */
    fprintf(stderr, "caster output: host = %s, port = %d, mountpoint = %s\n",
      inet_ntoa(out_addr.sin_addr), out_port, mountpoint);
    if(connect(sock_id, (struct sockaddr *) &out_addr, sizeof(out_addr)) < 0)
    {
      fprintf(stderr, "ERROR: can't connect output to %s at port %d\n",
        inet_ntoa(out_addr.sin_addr), out_port);
      close(sock_id);
      exit(3);
    }

    /* set socket buffer size */
    setsockopt(sock_id, SOL_SOCKET, SO_SNDBUF, (const char *) &size,
      sizeof(const char *));
    /* send message to caster */
    szSendBuffer[0] = '\0';
    sprintf(szSendBuffer, "SOURCE %s /%s\r\n", password, mountpoint);
    strcat(szSendBuffer, "Source-Agent: ");
    strcat(szSendBuffer, VERSION);
    strcat(szSendBuffer, "\r\n");
    strcat(szSendBuffer, "\r\n");
    strcat(szSendBuffer, "\0");
    nBufferBytes = strlen(szSendBuffer);
    if((send(sock_id, szSendBuffer, (size_t)nBufferBytes, 0)) != nBufferBytes)
    {
      fprintf(stderr, "ERROR: could not send to caster\n");
      close(sock_id);
      sleep(5);
      exit(0);
    }
    /* check caster's response */
    nBufferBytes = recv(sock_id, szSendBuffer, sizeof(szSendBuffer), 0);
    szSendBuffer[nBufferBytes] = '\0';
    if(strcmp(szSendBuffer, "OK\r\n"))
    {
      char *a;
      fprintf(stderr, "ERROR: caster's reply is not OK : ");
      for(a = szSendBuffer; *a && *a != '\n' && *a != '\r'; ++a)
      {
        fprintf(stderr, "%.1s", isprint(*a) ? a : ".");
      }
      fprintf(stderr, "\n");
      close(sock_id);
      sleep(5);
      exit(0);
    }
    printf("connection succesfull\n");
    send_receive_loop(sock_id, gpsfd);
  }
  exit(0);
}

static void send_receive_loop(int sock, int fd)
{
  char buffer[BUFSZ] = { 0 };
  int nBufferBytes = 0, i;
  /* data transmission */
  printf("transfering data ...\n");
  while(1)
  {
    if(!nBufferBytes)
    {
      /* receiving data */
      nBufferBytes = read(fd, buffer, BUFSZ);
      if(!nBufferBytes)
      {
        printf("WARNING: no data received from input\n");
        continue;
      }
      else if(nBufferBytes < 0)
      {
        perror("ERROR: reading input failed");
        exit(1);
      }
    }
    /* send data */
    if((i = send(sock, buffer, (size_t)nBufferBytes, MSG_DONTWAIT))
    != nBufferBytes)
    {
      if(i < 0 && errno != EAGAIN)
      {
        perror("WARNING: could not send data - retry connection");
        close(sock);
        sleep(5);
        return;
      }
      else if(i)
      {
        memmove(buffer, buffer+i, (size_t)(nBufferBytes-i));
        nBufferBytes -= i;
      }
    }
    else
    {
      nBufferBytes = 0;
    }
  }
}

/*
 * openserial
 *
 * Open the serial port with the given device name and configure it for
 * reading NMEA data from a GPS receiver.
 *
 * Parameters:
 *     tty     : pointer to    : A zero-terminated string containing the device
 *               unsigned char   name of the appropriate serial port.
 *     blocksz : integer       : Block size for port I/O
 *     baud :    integer       : Baud rate for port I/O
 *
 * Return Value:
 *     The function returns a file descriptor for the opened port if successful.
 *     The function returns -1 in the event of an error.
 *
 * Remarks:
 *
 */

static int openserial(const char * tty, int blocksz, int baud)
{
  int fd;
  struct termios termios;

  fd = open(tty, O_RDWR | O_NONBLOCK | O_EXLOCK);
  if(fd < 0)
  {
    perror("ERROR: opening serial connection");
    return (-1);
  }
  if(tcgetattr(fd, &termios) < 0)
  {
    perror("ERROR: get serial attributes");
    return (-1);
  }
  termios.c_iflag = 0;
  termios.c_oflag = 0;          /* (ONLRET) */
  termios.c_cflag = CS8 | CLOCAL | CREAD;
  termios.c_lflag = 0;
  {
    int cnt;
    for(cnt = 0; cnt < NCCS; cnt++)
      termios.c_cc[cnt] = -1;
  }
  termios.c_cc[VMIN] = blocksz;
  termios.c_cc[VTIME] = 2;

#if (B4800 != 4800)
  /*
   * Not every system has speed settings equal to absolute speed value.
   */

  switch (baud)
  {
  case 300:
    baud = B300;
    break;
  case 1200:
    baud = B1200;
    break;
  case 2400:
    baud = B2400;
    break;
  case 4800:
    baud = B4800;
    break;
  case 9600:
    baud = B9600;
    break;
  case 19200:
    baud = B19200;
    break;
  case 38400:
    baud = B38400;
    break;
#ifdef B57600
  case 57600:
    baud = B57600;
    break;
#endif
#ifdef B115200
  case 115200:
    baud = B115200;
    break;
#endif
#ifdef B230400
  case 230400:
    baud = B230400;
    break;
#endif
  default:
    fprintf(stderr, "WARNING: Baud settings not useful, using 19200\n");
    baud = B19200;
    break;
  }
#endif

  if(cfsetispeed(&termios, baud) != 0)
  {
    perror("ERROR: setting serial speed with cfsetispeed");
    return (-1);
  }
  if(cfsetospeed(&termios, baud) != 0)
  {
    perror("ERROR: setting serial speed with cfsetospeed");
    return (-1);
  }
  if(tcsetattr(fd, TCSANOW, &termios) < 0)
  {
    perror("ERROR: setting serial attributes");
    return (-1);
  }
  if(fcntl(fd, F_SETFL, 0) == -1)
  {
    perror("WARNING: setting blocking mode failed");
  }
  return (fd);
}

/*
 * usage
 *
 * Send a usage message to standard error and quit the program.
 *
 * Parameters:
 *     None.
 *
 * Return Value:
 *     The function does not return a value.
 *
 * Remarks:
 *
 */

static void usage(int rc)
{
  fprintf(stderr, "Usage: %s [OPTIONS]\n", VERSION);
  fprintf(stderr, "  Options are:\n");
  fprintf(stderr, "    -a caster name or address (default: %s)\n",
    NTRIP_CASTER);
  fprintf(stderr, "    -p caster port (default: %d)\n", NTRIP_PORT);
  fprintf(stderr, "    -m caster mountpoint\n");
  fprintf(stderr, "    -c password for caster login\n");
  fprintf(stderr, "    -h|? print this help screen\n");
  fprintf(stderr, "    -M <mode>  sets the mode\n");
  fprintf(stderr, "               (1=serial, 2=tcpsocket, 3=file)\n");
  fprintf(stderr, "  Mode = file:\n");
  fprintf(stderr, "    -s file, simulate data stream by reading log file\n");
  fprintf(stderr, "       default/current setting is %s\n", filepath);
  fprintf(stderr, "  Mode = serial:\n");
  fprintf(stderr, "    -b baud_rate, sets serial input baud rate\n");
  fprintf(stderr, "       default/current value is %d\n", ttybaud);
  fprintf(stderr, "    -i input_device, sets name of serial input device\n");
  fprintf(stderr, "       default/current value is %s\n", ttyport);
  fprintf(stderr, "       (normally a symbolic link to /dev/tty\?\?)\n");
  fprintf(stderr, "  Mode = tcpsocket:\n");
  fprintf(stderr, "    -P receiver port (default: 1025)\n");
  fprintf(stderr, "    -H hostname of TCP server (default: 127.0.0.1)\n");
  fprintf(stderr, "    -f initfile send to server\n");
  fprintf(stderr, "    \n");
  exit(rc);
}
