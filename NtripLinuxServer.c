/*
 * main.c
 *
 * Copyright (c) 2003
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

/* $Id$
 * Changes -  Version 0.7
 * Thu Sep 22 08:10:45  2003    actina AG <http://www.actina.de>
 * 
 *         Steffen Tschirpke <St.Tschirpke@actina.de>
 *         * main.c
 *           - socket support
 *           - command line option handling
 *           - error handling
 *           - help screen
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <getopt.h>
#include <sys/termios.h>
#include <fcntl.h>
#include "NtripServerLinux.h"

#define VERSION "NTRIP NtripServerLinux/0.8"

#define SIMULATE  0
#define SERIAL    1
#define TCPSOCKET 2

/* default socket source */
#define SERV_HOST_ADDR "127.0.0.1"
#define SERV_TCP_PORT 1025

/* default destination */
#define NTRIP_CASTER "129.217.182.51"
#define NTRIP_PORT 80

int verbose = 0;
int simulate = 0;
int tickinterval = 1;
int ttybaud = 19200;
char * ttyport = "/dev/gps";
int mode = 0;

/* Forward references */
int openserial (u_char * tty, int blocksz, int ttybaud);
int send_receive_loop(int socket, int fd);
void usage (int);

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

int main (int argc, char ** argv)
{
  u_char       * ttyin = ttyport;
  char         * logfilepath = NULL;
  FILE         * fp = NULL;
  int            c, gpsfd = -1;
  int            size = 2048;		//for setting send buffer size
  
  unsigned int out_port = 0;
  unsigned int in_port = 0;
  char         *mountpoint = NULL;
  char         *password = NULL;
  int          sock_id;
  char         szSendBuffer[BUFSZ];
  int          nBufferBytes;
  int          sockfd;

  struct hostent *inhost;
  struct hostent *outhost;
  
  struct sockaddr_in in_addr;
  struct sockaddr_in out_addr;

  //get and check program arguments
  if (argc <=1)
    {
      usage(2);
      exit(1);
    }
  while ((c = getopt (argc, argv, "M:i:h:b:p:s:a:m:c:H:P:")) != EOF)
    {
      switch (c)
        {
        case 'M':
          mode = atoi(optarg);
          if ((mode == 0) || (mode > 3))
            {
              fprintf (stderr, "can't convert %s to a valid mode\n\n", optarg);
              usage(-1);
            }
          break;
       	case 'i':		/* gps serial ttyin */
          ttyin = optarg;
          break;
        case 'b':		/* serial ttyin speed */
          ttybaud = atoi (optarg);
          if (ttybaud <= 1)
            {
              fprintf (stderr, "can't convert %s to valid ttyin speed\n\n", optarg);
              usage(1);
            }
          break;
        case 'a':		/* http server IP address A.B.C.D*/
          outhost = gethostbyname(optarg);
          if (outhost == NULL)
            {
              fprintf (stderr, "host %s unknown\n\n", optarg);
              usage(-2);
            }
          memset((char *) &out_addr, 0x00, sizeof(out_addr));
          memcpy(&out_addr.sin_addr, outhost->h_addr, outhost->h_length);
          break;
        case 'p':		/* http server port*/
          out_port = atoi(optarg);
          if (out_port <= 1)
            {
              fprintf (stderr, "can't convert %s to a valid http server port\n\n", optarg);
              usage(1);
            }
          break;
        case 'm':		/* http server mountpoint*/
          mountpoint = optarg;
          break;
        case 's':		/* simulate datastream from file */
          logfilepath = optarg;
          break;
        case 'c':               /* password */
          password=optarg;
          break;
        case 'H':               /* host */
          inhost = gethostbyname(optarg);
          if (inhost == NULL)
            {
              /* TODO Errorhandling/Debugging - Output */
              fprintf (stderr, "host %s unknown\n\n", optarg);
              usage(-2);
            }
          memset((char *) &in_addr, 0x00, sizeof(in_addr));
          memcpy(&in_addr.sin_addr, inhost->h_addr, inhost->h_length);
          break;
        case 'P':               /* port */
          in_port = atoi(optarg);
          if (in_port <= 1)
            {
              /* TODO Errorhandling/Debugging - Output */
              fprintf (stderr, "can't convert %s to a valid receiver port\n\n", optarg);
              usage(1);
            }
          break;
        case 'h':               /* help */
        case '?':
          usage(0);
          break;
        default:
          usage(2);
          break;
        }
      
      if (in_port <= 0 )
        {
          in_port = SERV_TCP_PORT;
        }
      if (out_port <= 0 )
        {
          out_port = NTRIP_PORT;
        }
    }
 
  argc -= optind;
  argv += optind;
  
  if (argc > 0)
    {
      /* TODO Errorhandling/Debugging - Output */
      fprintf (stderr, "Extra args on command line.\n");
      for (; argc > 0; argc--)
        {
          fprintf (stderr, " %s", *argv++);
        }
      fprintf (stderr, "\n");
      usage (1);		/* never returns */
    }
  
  if ((mode ==SIMULATE) && (tickinterval <= 0))
    {
      /* TODO Errorhandling/Debugging - Output */
      fprintf (stderr, "-t parameter must be positive in simulator mode\n");
      exit (1);
    }
  
  if (verbose >= 1)
    fprintf (stderr, "%s\n", VERSION);
  if (mountpoint == NULL)
    {
      /* TODO Errorhandling/Debugging - Output */
      fprintf(stderr,"missing mountpoint arguement\n");
      exit(1);
    }
  //print program arguments on screen
  /* TODO Errorhandling/Debugging - Output */
  // printf("\ncaster IP :\t%s\nport :\t%d\nmountpoint :\t%s\npassword :\t%s\n\n",address,port,mountpoint,(password==NULL)?"none":"yes");
  if (password == NULL)
    {
      password="\0";				
    }
  
  switch (mode)
    {
    case SIMULATE:
      {
        fp = fopen (logfilepath, "r");
        if (!fp)
          {
            /* TODO Errorhandling/Debugging - Output */
            perror ("fopen logfile");
            exit (1);
          }
      }
      break;
    case SERIAL:          //open serial port
      {
        gpsfd = openserial (ttyin, 1, ttybaud);
        fp = fdopen (gpsfd, "r");
        if (!fp)
          {
            /* TODO Errorhandling/Debugging - Output */
            perror ("fdopen gps");
            exit (1);
          }
      }
      break;
    case TCPSOCKET:
      {
        in_addr.sin_family      = AF_INET;
        in_addr.sin_port        = htons(in_port);  
        
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
          {
            /* TODO Errorhandling/Debugging - Output */
            fprintf (stderr, "can't create socket\n");
            exit(1);
          }
        
        /* TODO Errorhandling/Debugging - Output */
        printf("connecting input ...\n");
        fprintf (stderr, "socket input:\n");
        fprintf (stderr, "\tHost: %s\n", inet_ntoa(in_addr.sin_addr));
        fprintf (stderr, "\tPort: %d\n", in_port);
        
        if (connect(sockfd, (struct sockaddr *) &in_addr, sizeof(in_addr)) < 0)
          {
            /* TODO Errorhandling/Debugging - Output */
            fprintf (stderr, "can't connect input to %s at port %d\n", inet_ntoa(in_addr.sin_addr), in_port);
            //  exit(1);
          } 
        gpsfd = sockfd;
      }
      break;
    default:
      usage(-1);
      break;
    }
  
  out_addr.sin_family = AF_INET;
  out_addr.sin_port = htons((u_short)(out_port));

  // ----- main part -----
  while(1)
    {
      //create socket
      if ((sock_id = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
          /* TODO Errorhandling/Debugging - Output */
          printf("ERROR : could not create socket\n");
          // sleep(5);
          exit(2);
  	}
      //connect to caster
      printf("connecting output ...\n");
      fprintf (stderr, "caster output:\n");
      fprintf (stderr, "\tHost: %s\n", inet_ntoa(out_addr.sin_addr));
      fprintf (stderr, "\tPort: %d\n", out_port);
      if (connect(sock_id, (struct sockaddr *) &out_addr, sizeof(out_addr)) < 0)
        {
          /* TODO Errorhandling/Debugging - Output */
          fprintf (stderr, "can't connect output to %s at port %d\n", inet_ntoa(out_addr.sin_addr), out_port);
          close(sock_id);
          // sleep(5);
          exit(3);
  	}
	
      /* TODO Errorhandling/Debugging - Output */
      printf("connection succesfull\n");
      //set socket buffer size
      setsockopt(sock_id,SOL_SOCKET,SO_SNDBUF,(const char *) &size,sizeof(const char *));
      //send message to caster
      szSendBuffer[0] = '\0';
      sprintf(szSendBuffer,"SOURCE %s /%s\r\n",password,mountpoint);
      strcat(szSendBuffer,"Source-Agent: ");
      strcat(szSendBuffer,VERSION);
      strcat(szSendBuffer, "\r\n");
      strcat(szSendBuffer, "\r\n");
      strcat(szSendBuffer, "\0");
      nBufferBytes=strlen(szSendBuffer);
      if ((send(sock_id, szSendBuffer, nBufferBytes, 0)) != nBufferBytes)
        {
          /* TODO Errorhandling/Debugging - Output */
	  fprintf(stderr, "ERROR : could not send to caster\n");		
          close(sock_id);
          sleep(5);
          exit(0);
        }
      //check caster's response
      nBufferBytes=recv(sock_id,szSendBuffer,sizeof(szSendBuffer),0);
      szSendBuffer[nBufferBytes]='\0';
      if(strcmp(szSendBuffer,"OK\r\n"))
        {
          /* TODO Errorhandling/Debugging - Output */
          fprintf(stderr,"caster's reply is not OK\n");	
          close(sock_id);
          sleep(5);
          exit(0);
        }
      send_receive_loop(sock_id, gpsfd);
    }
  exit(0);
}

int send_receive_loop(int socket, int fd)
{
  char buffer[BUFSZ]={0};
  int nBufferBytes=0;
  //data transmission
  printf("transfering data ...\n");
  while(1)
    {
      // recieving data
      nBufferBytes = read(fd, buffer, BUFSZ);
      if (nBufferBytes<=0)
        {
          /* TODO Errorhandling/Debugging - Output */
          printf("ERROR : no data recieved from serial port or socket\n");
        }
      // send data
      if((send(socket, buffer, nBufferBytes, MSG_DONTWAIT)) != nBufferBytes)
        {                                       //MSG_DONTWAIT  : send works in non-blocking mode
          /* TODO Errorhandling/Debugging - Output */
          fprintf(stderr,"ERROR : could not send data\n");		
          close(socket);
          sleep(5);
          exit(4);
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
 *     ttybaud : integer       : Baud rate for port I/O
 *
 * Return Value:
 *     The function returns a file descriptor for the opened port if successful.
 *     The function returns -1 in the event of an error.
 *
 * Remarks:
 *
 */

int openserial (u_char * tty, int blocksz, int ttybaud)
{
  int             fd;
  struct termios  termios;
  
  fd = open(tty, O_RDWR | O_NONBLOCK
#ifdef O_EXLOCK        		/* for Linux */
            | O_EXLOCK
#endif
            );
  if (fd < 0) {
    perror("open");
    return (-1);
  }
  if (tcgetattr(fd, &termios) < 0) {
    
    perror("tcgetattr");
    return (-1);
  }
  termios.c_iflag = 0;
  termios.c_oflag = 0;        /* (ONLRET) */
  termios.c_cflag = CS8 | CLOCAL | CREAD;
  termios.c_lflag = 0;
  {
    int   cnt;
    for (cnt = 0; cnt < NCCS; cnt++)
      termios.c_cc[cnt] = -1;
  }
  termios.c_cc[VMIN] = blocksz;
  termios.c_cc[VTIME] = 2;
  
#if (B4800 != 4800)
  /*
   * Only paleolithic systems need this.
   */
  
  switch (ttybaud)
    {
    case 300:
      ttybaud = B300;
      break;
    case 1200:
      ttybaud = B1200;
      break;
    case 2400:
      ttybaud = B2400;
      break;
    case 4800:
      ttybaud = B4800;
      break;
    case 9600:
      ttybaud = B9600;
      break;
    case 19200:
      ttybaud = B19200;
      break;
    case 38400:
      ttybaud = B38400;
      break;
    default:
      ttybaud = B19200;
      break;
    }
#endif
  
  if (cfsetispeed(&termios, ttybaud) != 0)
    {
      perror("cfsetispeed");
      return (-1);
    }
  if (cfsetospeed(&termios, ttybaud) != 0)
    {
      perror("cfsetospeed");
      return (-1);
    }
  if (tcsetattr(fd, TCSANOW, &termios) < 0)
    {
      perror("tcsetattr");
      return (-1);
    }
#if 1        			/* WANT_BLOCKING_READ */
  if (fcntl(fd, F_SETFL, 0) == -1)
    {
      perror("fcntl: set nonblock");
    }
#endif
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

void usage (int rc)
{
  fprintf (stderr, "%s: application\n",VERSION);
  fprintf (stderr, "Usage: %s [OPTIONS]\n",VERSION);
  fprintf (stderr, "  Options are:\n");
  fprintf (stderr, "    -a caster name or address (default: %s)\n", NTRIP_CASTER);
  fprintf (stderr, "    -p caster port (default: %d)\n", NTRIP_PORT);
  fprintf (stderr, "    -m caster mountpoint\n");
  fprintf (stderr, "    -c password for caster login\n");
  fprintf (stderr, "    -h|? print this help screen\n");
  fprintf (stderr, "    -M <mode>  sets the mode\n");
  fprintf (stderr, "               (1=serial, 2=tcpsocket, 3=simulate)\n");
  fprintf (stderr, "  Mode = simulate:\n");
  fprintf (stderr, "    -s file, simulate data stream by reading log file\n");
  fprintf (stderr, "       default/current setting is %s\n",  (simulate ? "enabled" : "disabled"));
  fprintf (stderr, "  Mode = serial:\n");
  fprintf (stderr, "    -b baud_rate, sets serial input baud rate\n");
  fprintf (stderr, "       default/current value is %d\n", ttybaud);
  fprintf (stderr, "    -i input_device, sets name of serial input device\n");
  fprintf (stderr, "       default/current value is %s\n", ttyport);
  fprintf (stderr, "       (normally a symbolic link to /dev/tty\?\?)\n");
  fprintf (stderr, "  Mode = tcpsocket:\n");
  fprintf (stderr, "    -P receiver port (default: 1025)\n");
  fprintf (stderr, "    -H hostname of TCP server (default: 127.0.0.1)\n");
  fprintf (stderr, "    \n");
  exit (rc);
}








