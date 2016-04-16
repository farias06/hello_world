#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include "libpq-fe.h"

#define MYPORT 80
#define BACKLOG 5
#define MAXCLIENTS 5
#define MAXDATASIZE 100

int
main (void)
{
  int sockfd = -1, new_fd, numbytes, highest = 0, i;
  int clients[MAXCLIENTS];
  char buffer[MAXDATASIZE];
  char localip[MAXDATASIZE];
  struct sockaddr_in my_addr, their_addr;
  socklen_t sin_size;
  struct timeval tv;
  fd_set readfds;
  const char *conninfo;
  PGconn *conn;
  PGresult *res;
  FILE *f;
  char line[100], *p, *c;
  const char *google_dns_server = "8.8.8.8";
  int dns_port = 53;
  struct sockaddr_in serv;
  int sock = socket (AF_INET, SOCK_DGRAM, 0);

  f = fopen ("/proc/net/route", "r");

  while (fgets (line, 100, f))
    {
      p = strtok (line, " \t");
      c = strtok (NULL, " \t");

      if (p != NULL && c != NULL)
	{
	  if (strcmp (c, "00000000") == 0)
	    {
	      printf ("Default interface is : %s \n", p);
	      break;
	    }
	}
    }

  if (sock < 0)
    {
      perror ("Socket error");
    }

  memset (&serv, 0, sizeof (serv));
  serv.sin_family = AF_INET;
  serv.sin_addr.s_addr = inet_addr (google_dns_server);
  serv.sin_port = htons (dns_port);

  int err = connect (sock, (const struct sockaddr *) &serv, sizeof (serv));

  struct sockaddr_in name;
  socklen_t namelen = sizeof (name);
  err = getsockname (sock, (struct sockaddr *) &name, &namelen);

  const char *p2 = inet_ntop (AF_INET, &name.sin_addr, localip, 100);

  if (p2 != NULL)
    {
      printf ("Local ip is : %s \n", localip);
    }
  else
    {
      //Some error
      printf ("Error number : %d . Error message : %s \n", errno,
	      strerror (errno));
      strcpy (localip, "Error");
    }

  close (sock);

  /*
  conninfo =
    "hostaddr=127.0.0.1 port=5432 dbname=postgres user=postgres password=password";
   */
   conninfo =
    "hostaddr=$POSTGRES2_PORT_5432_TCP_ADDR port=$POSTGRES2_PORT_5432_TCP_PORT user=postgres password=$POSTGRES2_ENV_POSTGRES_PASSWORD";

  if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("socket");
      exit (-1);
    }
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons (MYPORT);
  my_addr.sin_addr.s_addr = INADDR_ANY;
  bzero (&(my_addr.sin_zero), 8);

  if (bind (sockfd, (struct sockaddr *) &my_addr, sizeof (struct sockaddr)) ==
      -1)
    {
      perror ("bind");
      exit (-1);
    }
  if (listen (sockfd, BACKLOG) == -1)
    {
      perror ("listen");
      exit (-1);
    }
  bzero (clients, sizeof (clients));
  highest = sockfd;
  while (1)
    {
      sin_size = sizeof (struct sockaddr_in);
      tv.tv_sec = 0;
      tv.tv_usec = 250000;
      FD_ZERO (&readfds);
      for (i = 0; i < MAXCLIENTS; i++)
	{
	  if (clients[i] != 0)
	    {
	      FD_SET (clients[i], &readfds);
	    }
	}
      FD_SET (sockfd, &readfds);
      if (select (highest + 1, &readfds, NULL, NULL, &tv) >= 0)
	{
	  if (FD_ISSET (sockfd, &readfds))
	    {
	      if ((new_fd =
		   accept (sockfd, (struct sockaddr *) &their_addr,
			   &sin_size)) == -1)
		{
		  perror ("ACCEPT");
		  continue;
		}
	      for (i = 0; i < MAXCLIENTS; i++)
		{
		  if (clients[i] == 0)
		    {
		      clients[i] = new_fd;
		      break;
		    }
		}
	      if (i != MAXCLIENTS)
		{
		  if (new_fd > highest)
		    {
		      highest = clients[i];
		    }
		  printf ("Connexion received from %s (slot %i) ",
			  inet_ntoa (their_addr.sin_addr), i);
		  send (new_fd, "\nHELLO\n", 7, MSG_NOSIGNAL);
		}
	      else
		{
		  send (new_fd, "\nTOO MANY CLIENT\n", 17, MSG_NOSIGNAL);
		  close (new_fd);
		}
	    }
	  for (i = 0; i < MAXCLIENTS; i++)
	    {
	      if (FD_ISSET (clients[i], &readfds))
		{
		  if ((numbytes =
		       recv (clients[i], buffer, MAXDATASIZE, 0)) <= 0)
		    {
		      printf ("Connexion lost from slot %i", i);
		      close (clients[i]);
		      clients[i] = 0;
		    }
		  else
		    {
		      buffer[numbytes] = '\0';
		      printf ("Received from slot %i : %s", i, buffer);
		      if (strncmp (buffer, "POSTGRES", 6) == 0)
			{
			  conn = PQconnectdb (conninfo);
			  if (PQstatus (conn) != CONNECTION_OK)
			    {
			      fprintf (stderr,
				       "Connection to database failed: %s",
				       PQerrorMessage (conn));
			      send (new_fd, "\nDB KO\n", 7, MSG_NOSIGNAL);
			    }
			  else
			    {
			      send (new_fd, "\nDB OK\n", 7, MSG_NOSIGNAL);
			      /* INSERT CLIENT IP and timestamp */
			    }
			  PQfinish (conn);
			}
		      if ((strncmp (buffer, "QUIT", 4) == 0))
			{
			  printf ("Connexion QUIT from slot %i", i);
			  close (clients[i]);
			  clients[i] = 0;
			}
		      if ((strncmp (buffer, "EXIT", 4) == 0))
			{
			  printf ("Connexion EXIT from slot %i", i);
			  close (clients[i]);
			  clients[i] = 0;
			}
		      if ((strncmp (buffer, "CLOSE", 5) == 0))
			{
			  printf ("Connexion CLOSE from slot %i", i);
			  close (clients[i]);
			  clients[i] = 0;
			}
		      if ((strncmp (buffer, "INTERFACE", 9) == 0))
			{
			  send (new_fd, "\n", 1, MSG_NOSIGNAL);
			  send (new_fd, localip, strlen (localip),
				MSG_NOSIGNAL);
			  send (new_fd, "\n", 1, MSG_NOSIGNAL);
			}
		      if ((strncmp (buffer, "IP", 2) == 0))
			{
			  send (new_fd, "\n", 1, MSG_NOSIGNAL);
			  send (new_fd, p, strlen (p), MSG_NOSIGNAL);
			  send (new_fd, "\n", 1, MSG_NOSIGNAL);
			}
                      if ((strncmp (buffer, "DBCNX", 2) == 0))
                        {
                          send (new_fd, "\n", 1, MSG_NOSIGNAL);
                          send (new_fd, conninfo, strlen (conninfo), MSG_NOSIGNAL);
                          send (new_fd, "\n", 1, MSG_NOSIGNAL);
                        }
		    }
		}
	    }
	}
      else
	{
	  perror ("SELECT");
	  continue;
	}
    }
  return 0;
}
