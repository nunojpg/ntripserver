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

/* $Id: NtripLinuxServer.c,v 1.23 2006/08/16 08:33:04 stoecker Exp $
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
 *           - Modes may be symbolic and not only numeric
 *
 * Changes - Version 0.11
 * Jun 02 2005  Dirk Stoecker <soft@dstoecker.de>
 *           - added SISNeT support
 *           - added UDP support
 *           - cleanup of host and port handling
 *           - added inactivity alarm of 60 seconds
 *
 * Changes - Version 0.12
 * Jun 07 2005  Dirk Stoecker <soft@dstoecker.de>
 *           - added UDP bindmode
 *
 * Changes - Version 0.13
 * Apr 25 2006  Andrea Stuerze <andrea.stuerze@bkg.bund.de>
 *           - added stream retrieval from caster
 *
 * Changes - Version 0.14
 * May 16 2006  Andrea Stuerze <andrea.stuerze@bkg.bund.de>
 *           - bug fix in base64_encode-function
 *
 * Changes - Version 0.15
 * Jun 02 2006  Georg Weber <georg.weber@bkg.bund.de>
 *           - modification for SISNeT 3.1 protocol
 *
 * Changes - Version 0.16
 * Jul 06 2006 Andrea Stuerze <andrea.stuerze@bkg.bund.de>
 *           - more flexible caster's response
 *
 * Changes - Version 0.17
 * Jul 27 2006  Dirk Stoecker <soft@dstoecker.de>
 *           - fixed some problems with caster download
 *           - some minor cosmetic changes
 *
 * Changes - Version 0.18
 * Nov 23 2006  Dirk Stoecker <soft@dstoecker.de>
 *           - default port changed from 80 to 2101
 *
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <netdb.h>
#include <signal.h>
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

enum MODE { SERIAL = 1, TCPSOCKET = 2, INFILE = 3, SISNET = 4, UDPSOCKET = 5,
CASTER = 6, LAST};

#define VERSION         "NTRIP NtripServerLinux/0.17"
#define BUFSZ           1024

/* default socket source */
#define SERV_HOST_ADDR  "localhost"
#define SERV_TCP_PORT   2101

/* default destination */
#define NTRIP_CASTER    "www.euref-ip.net"
#define NTRIP_PORT     2101

/* default sisnet source */
#define SISNET_SERVER   "131.176.49.142"
#define SISNET_PORT     7777

#define ALARMTIME       60

static int ttybaud             = 19200;
static const char *ttyport     = "/dev/gps";
static const char *filepath    = "/dev/stdin";
static enum MODE mode          = INFILE;
static int sisnet              = 31;
static int gpsfd               = -1;

/* Forward references */
static int openserial(const char * tty, int blocksz, int baud);
static void send_receive_loop(int sock, int fd);
static void usage(int);
static int encode(char *buf, int size, const char *user, const char *pwd);

#ifdef __GNUC__
static __attribute__ ((noreturn)) void sighandler_alarm(
int sig __attribute__((__unused__)))
#else /* __GNUC__ */
static void sighandler_alarm(int sig)
#endif /* __GNUC__ */
{
  fprintf(stderr, "ERROR: more than %d seconds no activity\n", ALARMTIME);
  exit(1);
}

/*
* main
*
* Main entry point for the program.  Processes command-line arguments and
* prepares for action.
*
* Parameters:
*     argc : integer        : Number of command-line arguments.
*     argv : array of char  : Command-line arguments as an array of
*                             zero-terminated pointers to strings.
*
* Return Value:
*     The function does not return a value (although its return type is int).
*
* Remarks:
*
*/

int main(int argc, char **argv)
{
  int c;
  int size = 2048;              /* for setting send buffer size */

  const char *inhost = 0;
  const char *outhost = 0;
  unsigned int outport = 0;
  unsigned int inport = 0;
  const char *mountpoint = NULL;
  const char *password = "";
  const char *sisnetpassword = "";
  const char *sisnetuser = "";
  
  const char *stream_name=0;
  const char *stream_user=0;
  const char *stream_password=0;
  
  const char *initfile = NULL;
  int bindmode = 0;
  int sock_id;
  char szSendBuffer[BUFSZ];
  int nBufferBytes;
  struct hostent *he;
  struct sockaddr_in addr;

  signal(SIGALRM,sighandler_alarm);
  alarm(ALARMTIME);
  /* get and check program arguments */
  if(argc <= 1)
  {
    usage(2);
    exit(1);
  }
  while((c = getopt(argc, argv, "M:i:h:b:p:s:a:m:c:H:P:f:l:u:V:D:U:W:B"))
  != EOF)
  {
    switch (c)
    {
    case 'M':
      if(!strcmp(optarg, "serial")) mode = SERIAL;
      else if(!strcmp(optarg, "tcpsocket")) mode = TCPSOCKET;
      else if(!strcmp(optarg, "file")) mode = INFILE;
      else if(!strcmp(optarg, "sisnet")) mode = SISNET;
      else if(!strcmp(optarg, "udpsocket")) mode = UDPSOCKET;
      else if(!strcmp(optarg, "caster")) mode = CASTER;
      else mode = atoi(optarg);
      if((mode == 0) || (mode >= LAST))
      {
        fprintf(stderr, "ERROR: can't convert %s to a valid mode\n", optarg);
        usage(-1);
      }
      break;
    case 'i':                  /* gps serial ttyport */
      ttyport = optarg;
      break;
    case 'B':
      bindmode = 1;
      break;
    case 'V':
      if(!strcmp("3.0", optarg)) sisnet = 30;
      else if(!strcmp("3.1", optarg)) sisnet = 31;
      else if(!strcmp("2.1", optarg)) sisnet = 21;
      else
      {
        fprintf(stderr, "ERROR: unknown SISNeT version %s\n", optarg);
        usage(-2);
      }
      break;
    case 'b':                  /* serial ttyin speed */
      ttybaud = atoi(optarg);
      if(ttybaud <= 1)
      {
        fprintf(stderr, "ERROR: can't convert %s to valid serial speed\n",
          optarg);
        usage(1);
      }
      break;
    case 'a':                  /* http server IP address A.B.C.D */
      outhost = optarg;
      break;
    case 'p':                  /* http server port */
      outport = atoi(optarg);
      if(outport <= 1 || outport > 65535)
      {
        fprintf(stderr,
          "ERROR: can't convert %s to a valid HTTP server port\n", optarg);
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
    case 'u':
      sisnetuser = optarg;
      break;
    case 'l':
      sisnetpassword = optarg;
      break;
    case 'c':                  /* password */
      password = optarg;
      break;
    case 'H':                  /* host */
      inhost = optarg;
      break;
    case 'P':                  /* port */
      inport = atoi(optarg);
      if(inport <= 1 || inport > 65535)
      {
        fprintf(stderr, "ERROR: can't convert %s to a valid port number\n",
          optarg);
        usage(1);
      }
      break;
    case 'D':
     stream_name=optarg;        /* desired stream from SourceCaster */
     break; 
    case 'U':
     stream_user=optarg;        /* username for desired stream */
     break;
    case 'W':
     stream_password=optarg;    /* passwd for desired stream */
     break;
    case 'h':                  /* help */
    case '?':
      usage(0);
      break;
    default:
      usage(2);
      break;
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
    fprintf(stderr,
      "WARNING: Missing password argument - are you really sure?\n");
  }

  if(stream_name && stream_user && !stream_password)
  {
    fprintf(stderr, "WARNING: Missing password argument for download"
      " - are you really sure?\n");
  }

  if(!outhost) outhost = NTRIP_CASTER;
  if(!outport) outport = NTRIP_PORT;

  switch(mode)
  {
  case INFILE:
    {
      if((gpsfd = open(filepath, O_RDONLY)) < 0)
      {
        perror("ERROR: opening input file");
        exit(1);
      }
      /* set blocking mode in case it was not set
        (seems to be sometimes for fifo's) */
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
  case TCPSOCKET: case UDPSOCKET: case SISNET: case CASTER:
    {
      if(mode == SISNET)
      {
        if(!inhost) inhost = SISNET_SERVER;
        if(!inport) inport = SISNET_PORT;
      }
      else if(mode == CASTER)
      {
        if(!inport) inport = NTRIP_PORT;
        if(!inhost) inhost = NTRIP_CASTER;
      }
      else if((mode == TCPSOCKET) || (mode == UDPSOCKET))
      {
        if(!inport) inport = SERV_TCP_PORT;
        if(!inhost) inhost = "127.0.0.1";
      }      

      if(!(he = gethostbyname(inhost)))
      {
        fprintf(stderr, "ERROR: host %s unknown\n", inhost);
        usage(-2);
      }

      if((gpsfd = socket(AF_INET, mode == UDPSOCKET
      ? SOCK_DGRAM : SOCK_STREAM, 0)) < 0)
      {
        fprintf(stderr, "ERROR: can't create socket\n");
        exit(1);
      }

      memset((char *) &addr, 0x00, sizeof(addr));
      if(!bindmode)
        memcpy(&addr.sin_addr, he->h_addr, (size_t)he->h_length);
      addr.sin_family = AF_INET;
      addr.sin_port = htons(inport);

      printf("%s input: host = %s, port = %d, %s%s%s%s%s\n",
      mode == CASTER ? "caster" : mode == SISNET ? "sisnet" :
      mode == TCPSOCKET ? "tcp socket" : "udp socket",
      bindmode ? "127.0.0.1" : inet_ntoa(addr.sin_addr),
      inport, stream_name ? "stream = " : "", stream_name ? stream_name : "",
      initfile ? ", initfile = " : "", initfile ? initfile : "",
      bindmode ? "binding mode" : "");

      if(bindmode)
      {
        if(bind(gpsfd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
        {
          fprintf(stderr, "ERROR: can't bind input to port %d\n", inport);
          exit(1);
        }
      }
      else if(connect(gpsfd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
      {
        fprintf(stderr, "ERROR: can't connect input to %s at port %d\n",
          inet_ntoa(addr.sin_addr), inport);
        exit(1);
      }
            
      if(stream_name) /* data stream from caster */
      {
        int init = 0;

        /* set socket buffer size */
        setsockopt(gpsfd, SOL_SOCKET, SO_SNDBUF, (const char *) &size,
          sizeof(const char *));
        if(stream_user && stream_password)
        {
          /* leave some space for login */
          nBufferBytes=snprintf(szSendBuffer, sizeof(szSendBuffer)-40,
          "GET /%s HTTP/1.0\r\n"
          "User-Agent: %s\r\n"
          "Authorization: Basic ", stream_name, VERSION);
          /* second check for old glibc */
          if(nBufferBytes > (int)sizeof(szSendBuffer)-40 || nBufferBytes < 0)
          {
            fprintf(stderr, "Requested data too long\n");
            exit(1);
          }
          nBufferBytes += encode(szSendBuffer+nBufferBytes,
            sizeof(szSendBuffer)-nBufferBytes-5, stream_user, stream_password);
          if(nBufferBytes > (int)sizeof(szSendBuffer)-5)
          {
            fprintf(stderr, "Username and/or password too long\n");
            exit(1);
          }
          snprintf(szSendBuffer+nBufferBytes, 5, "\r\n\r\n");
          nBufferBytes += 5;
        }
        else
        {
          nBufferBytes = snprintf(szSendBuffer, sizeof(szSendBuffer),
          "GET /%s HTTP/1.0\r\n"
          "User-Agent: %s\r\n"
          "\r\n", stream_name, VERSION);
        }
        if((send(gpsfd, szSendBuffer, (size_t)nBufferBytes, 0))
        != nBufferBytes)
        {
          fprintf(stderr, "ERROR: could not send to caster\n");
          exit(1);
        }
        nBufferBytes = 0;
        /* check caster's response */ 
        while(!init && nBufferBytes < (int)sizeof(szSendBuffer)
        && (nBufferBytes += recv(gpsfd, szSendBuffer,
        sizeof(szSendBuffer)-nBufferBytes, 0)) > 0)
        {
          if(strstr(szSendBuffer, "\r\n"))
          {
            if(!strncmp(szSendBuffer, "ICY 200 OK\r\n", 10))
              init = 1;
            else
            {
              int k;
              fprintf(stderr, "Could not get the requested data: ");
              for(k = 0; k < nBufferBytes && szSendBuffer[k] != '\n'
              && szSendBuffer[k] != '\r'; ++k)
              {
                fprintf(stderr, "%c", isprint(szSendBuffer[k])
                ? szSendBuffer[k] : '.');
              }
              fprintf(stderr, "\n");
              exit(1);
            }
          }
        }
        if(!init)
        {
          fprintf(stderr, "Could not init caster download.");
          exit(1);
        }
      } /* end data stream from caster */

      if(initfile && mode != SISNET)
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
    if(mode == SISNET)
    {
      int i, j;
      char buffer[1024];

      i = snprintf(buffer, sizeof(buffer), sisnet >= 30 ? "AUTH,%s,%s\r\n"
        : "AUTH,%s,%s", sisnetuser, sisnetpassword);
      if((send(gpsfd, buffer, (size_t)i, 0)) != i)
      {
        perror("ERROR: sending authentication");
        exit(1);
      }
      i = sisnet >= 30 ? 7 : 5;
      if((j = recv(gpsfd, buffer, i, 0)) != i && strncmp("*AUTH", buffer, 5))
      {
        fprintf(stderr, "ERROR: SISNeT connect failed:");
        for(i = 0; i < j; ++i)
        {
          if(buffer[i] != '\r' && buffer[i] != '\n')
          {
            fprintf(stderr, "%c", isprint(buffer[i]) ? buffer[i] : '.');
          }
        }
        fprintf(stderr, "\n");
        exit(1);
      }
      if(sisnet >= 31)
      {
        if((send(gpsfd, "START\r\n", 7, 0)) != i)
        {
          perror("ERROR: sending start command");
          exit(1);
        }
      }
    }
    break;
  default:
    usage(-1);
    break;
  }

  /* ----- main part ----- */
  for(;;)
  {
    if(!(he = gethostbyname(outhost)))
    {
      fprintf(stderr, "ERROR: host %s unknown\n", outhost);
      usage(-2);
    }

    /* create socket */
    if((sock_id = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      fprintf(stderr, "ERROR: could not create socket\n");
      exit(2);
    }

    memset((char *) &addr, 0x00, sizeof(addr));
    memcpy(&addr.sin_addr, he->h_addr, (size_t)he->h_length);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(outport);

    /* connect to caster */
    fprintf(stderr, "caster output: host = %s, port = %d, mountpoint = %s\n",
      inet_ntoa(addr.sin_addr), outport, mountpoint);
    if(connect(sock_id, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
      fprintf(stderr, "ERROR: can't connect output to %s at port %d\n",
        inet_ntoa(addr.sin_addr), outport);
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
      break;
    }
    /* check caster's response */
    nBufferBytes = recv(sock_id, szSendBuffer, sizeof(szSendBuffer), 0);
    szSendBuffer[nBufferBytes] = '\0';
    if(!strstr(szSendBuffer, "OK"))
    {
      char *a;
      fprintf(stderr, "ERROR: caster's reply is not OK : ");
      for(a = szSendBuffer; *a && *a != '\n' && *a != '\r'; ++a)
      {
        fprintf(stderr, "%.1s", isprint(*a) ? a : ".");
      }
      fprintf(stderr, "\n");
      break;
    }
    printf("connection successfull\n");
    send_receive_loop(sock_id, gpsfd);
  }
  close(sock_id);
  sleep(5);
  return 0;
}

static void send_receive_loop(int sock, int fd)
{
  int nodata = 0;
  char buffer[BUFSZ] = { 0 };
  char sisnetbackbuffer[200];
  int nBufferBytes = 0;
  /* data transmission */
  printf("transfering data ...\n");
  while(1)
  {
    if(!nodata) alarm(ALARMTIME);
    else nodata = 0;

    if(!nBufferBytes)
    {
      if(mode == SISNET && sisnet <= 30)
      {
        int i;
        /* a somewhat higher rate than 1 second to get really each block */
        /* means we need to skip double blocks sometimes */
        struct timeval tv = {0,700000};
        select(0, 0, 0, 0, &tv);
        memcpy(sisnetbackbuffer, buffer, sizeof(sisnetbackbuffer));
        i = (sisnet >= 30 ? 5 : 3);
        if((send(gpsfd, "MSG\r\n", i, 0)) != i)
        {
          perror("ERROR: sending data request");
          exit(1);
        }
      }
      /* receiving data */
      nBufferBytes = read(fd, buffer, sizeof(buffer));
      if(!nBufferBytes)
      {
        printf("WARNING: no data received from input\n");
	sleep(3);
        nodata = 1;
        continue;
      }
      else if(nBufferBytes < 0)
      {
        perror("ERROR: reading input failed");
        exit(1);
      }
      /* we can compare the whole buffer, as the additional bytes
         remain unchanged */
      if(mode == SISNET && sisnet <= 30 &&
      !memcmp(sisnetbackbuffer, buffer, sizeof(sisnetbackbuffer)))
      {
        nBufferBytes = 0;
      }
    }
    if(nBufferBytes)
    {
      int i;
      /* send data */
      if((i = send(sock, buffer, (size_t)nBufferBytes, MSG_DONTWAIT))
        != nBufferBytes)
      {
        if(i < 0)
        {
          if(errno != EAGAIN)
          {
            perror("WARNING: could not send data - retry connection");
            close(sock);
            sleep(5);
            return;
          }
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

static
#ifdef __GNUC__
__attribute__ ((noreturn))
#endif /* __GNUC__ */
void usage(int rc)
{
  fprintf(stderr, "Usage: %s [OPTIONS]\n", VERSION);
  fprintf(stderr, "  Options are: [-]           \n");
  fprintf(stderr, "    -a DestinationCaster name or address (default: %s)\n",
    NTRIP_CASTER);
  fprintf(stderr, "    -p DestinationCaster port (default: %d)\n", NTRIP_PORT);
  fprintf(stderr, "    -m DestinationCaster mountpoint\n");
  fprintf(stderr, "    -c DestinationCaster password\n");
  fprintf(stderr, "    -h|? print this help screen\n");
  fprintf(stderr, "    -M <mode>  sets the input mode\n");
  fprintf(stderr, "               (1=serial, 2=tcpsocket, 3=file, 4=sisnet"
    ", 5=udpsocket, 6=caster)\n");
  fprintf(stderr, "  Mode = file:\n");
  fprintf(stderr, "    -s file, simulate data stream by reading log file\n");
  fprintf(stderr, "       default/current setting is %s\n", filepath);
  fprintf(stderr, "  Mode = serial:\n");
  fprintf(stderr, "    -b baud_rate, sets serial input baud rate\n");
  fprintf(stderr, "       default/current value is %d\n", ttybaud);
  fprintf(stderr, "    -i input_device, sets name of serial input device\n");
  fprintf(stderr, "       default/current value is %s\n", ttyport);
  fprintf(stderr, "       (normally a symbolic link to /dev/tty\?\?)\n");
  fprintf(stderr, "  Mode = tcpsocket or udpsocket:\n");
  fprintf(stderr, "    -P receiver port (default: %d)\n", SERV_TCP_PORT);
  fprintf(stderr, "    -H hostname of TCP server (default: %s)\n",
    SERV_HOST_ADDR);
  fprintf(stderr, "    -f initfile send to server\n");
  fprintf(stderr, "    -B bindmode: bind to incoming UDP stream\n");
  fprintf(stderr, "  Mode = sisnet:\n");
  fprintf(stderr, "    -P receiver port (default: %d)\n", SISNET_PORT);
  fprintf(stderr, "    -H hostname of TCP server (default: %s)\n",
    SISNET_SERVER);
  fprintf(stderr, "    -u username\n");
  fprintf(stderr, "    -l password\n");
  fprintf(stderr, "    -V version [2.1, 3.0 or 3.1] (default: 3.1)\n");
  fprintf(stderr, "  Mode = caster:\n");
  fprintf(stderr, "    -P SourceCaster port (default: %d)\n", NTRIP_PORT);
  fprintf(stderr, "    -H SourceCaster hostname (default: %s)\n",
    NTRIP_CASTER);
  fprintf(stderr, "    -D SourceCaster mountpoint\n");
  fprintf(stderr, "    -U SourceCaster mountpoint username\n");
  fprintf(stderr, "    -W SourceCaster mountpoint password\n");  
  fprintf(stderr, "\n");
  exit(rc);
}

static const char encodingTable [64] = {
  'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
  'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
  'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
  'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'
};

/* does not buffer overrun, but breaks directly after an error */
/* returns the number of required bytes */
static int encode(char *buf, int size, const char *user, const char *pwd)
{
  unsigned char inbuf[3];
  char *out = buf;
  int i, sep = 0, fill = 0, bytes = 0;

  while(*user || *pwd)
  {
    i = 0;
    while(i < 3 && *user) inbuf[i++] = *(user++);
    if(i < 3 && !sep)    {inbuf[i++] = ':'; ++sep; }
    while(i < 3 && *pwd)  inbuf[i++] = *(pwd++);
    while(i < 3)         {inbuf[i++] = 0; ++fill; }
    if(out-buf < size-1)
      *(out++) = encodingTable[(inbuf [0] & 0xFC) >> 2];
    if(out-buf < size-1)
      *(out++) = encodingTable[((inbuf [0] & 0x03) << 4)
               | ((inbuf [1] & 0xF0) >> 4)];
    if(out-buf < size-1)
    {
      if(fill == 2)
        *(out++) = '=';
      else
        *(out++) = encodingTable[((inbuf [1] & 0x0F) << 2)
                 | ((inbuf [2] & 0xC0) >> 6)];
    }
    if(out-buf < size-1)
    {
      if(fill >= 1)
        *(out++) = '=';
      else
        *(out++) = encodingTable[inbuf [2] & 0x3F];
    }
    bytes += 4;
  }
  if(out-buf < size)
    *out = 0;
  return bytes;
}
