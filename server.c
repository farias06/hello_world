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
  struct sockaddr_in my_addr, their_addr;
  socklen_t sin_size;
  struct timeval tv;
  fd_set readfds;
  const char *conninfo;
  PGconn *conn;
  PGresult *res;
  int nFields;

  conninfo =
    "hostaddr=127.0.0.1 port=5432 dbname=Docker user=Docker password=Docker";

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
		  perror ("accept");
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
		  send (new_fd, "\nPING", 5, MSG_NOSIGNAL);
		}
	      else
		{
		  send (new_fd, "\nTOO MANY CLIENT", 16, MSG_NOSIGNAL);
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
			      send (new_fd, "\nDB KO", 6, MSG_NOSIGNAL);
			    }
			  else
			    {
			      send (new_fd, "\nDB OK", 6, MSG_NOSIGNAL);
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
