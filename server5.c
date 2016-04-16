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
#include <syslog.h>

#define MYPORT 80
#define BACKLOG 5
#define MAXCLIENTS 5
#define MAXDATASIZE 100

long
getMicrotime ()
{
  struct timeval currentTime;
  gettimeofday (&currentTime, NULL);
  return currentTime.tv_sec * (int) 1e6 + currentTime.tv_usec;
}

int
main (void)
{
  int sockfd = -1, new_fd, numbytes, highest = 0, i;
  int clients[MAXCLIENTS];
  long clients_t[MAXCLIENTS];
  char clients_ip[MAXCLIENTS][MAXDATASIZE];
  char buffer[MAXDATASIZE];
  char localip[MAXDATASIZE];
  struct sockaddr_in my_addr, their_addr;
  socklen_t sin_size;
  struct timeval tv;
  fd_set readfds;
  char conninfo[MAXDATASIZE];
  PGconn *conn;
  PGresult *res;
  FILE *f;
  char line[100], *p, *c;
  const char *google_dns_server = "8.8.8.8";
  int dns_port = 53;
  struct sockaddr_in serv;
  int sock = socket (AF_INET, SOCK_DGRAM, 0);

  setlogmask (LOG_UPTO (LOG_NOTICE));

  openlog ("server5", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

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
      syslog (LOG_ERR, "Error on socket");
      closelog ();
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
      printf ("Error number : %d . Error message : %s \n", errno,
	      strerror (errno));
      strcpy (localip, "Error");
    }

  close (sock);

  printf ("POSTGRES2_PORT_5432_TCP_ADDR : %s \n",
	  getenv ("POSTGRES2_PORT_5432_TCP_ADDR"));

  if (getenv ("POSTGRES2_PORT_5432_TCP_ADDR") == NULL)
    {
      sprintf (conninfo, "hostaddr=%s port=%s user=postgres password=%s",
	       "127.0.0.1", "5432", "password");
      syslog (LOG_NOTICE, "Local mode");
    }
  else
    {
      sprintf (conninfo, "hostaddr=%s port=%s user=postgres password=%s",
	       getenv ("POSTGRES2_PORT_5432_TCP_ADDR"),
	       getenv ("POSTGRES2_PORT_5432_TCP_PORT"),
	       getenv ("POSTGRES2_ENV_POSTGRES_PASSWORD"));
      syslog (LOG_NOTICE, "Docker mode");
    }

  conn = PQconnectdb (conninfo);
  if (PQstatus (conn) != CONNECTION_OK)
    {
      fprintf (stderr,
	       "Connection to database failed: %s \n", PQerrorMessage (conn));
    }
  else
    {
      res = PQexec (conn, "select count(*) from STATISTIQUE;");
      if (PQresultStatus (res) != PGRES_TUPLES_OK)
	{
	  printf ("KO : Table STATISTIQUE \n");
	  PQclear (res);
	  res =
	    PQexec (conn,
		    "CREATE TABLE STATISTIQUE ( ID serial primary key, CLIENT CHAR(20) NOT NULL, SERVER CHAR(20) NOT NULL, TEMPS_MILLI INT NOT NULL, TEMPS_CUR INT);");
	  if (PQresultStatus (res) != PGRES_COMMAND_OK)
	    {
	      printf ("KO : Table STATISTIQUE (CREATE) \n");
	      PQclear (res);
	    }
	  else
	    {
	      printf ("OK : Table STATISTIQUE (CREATE) \n");
	      PQclear (res);
	    }
	}
      else
	{
	  printf ("OK : Table STATISTIQUE \n");
	  PQclear (res);
	}
    }
  PQfinish (conn);

  syslog (LOG_NOTICE, "End of check of table");

  if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("socket");
      syslog (LOG_ERR, "Error on socket");
      closelog ();
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
      syslog (LOG_ERR, "Error on bind");
      closelog ();
      exit (-1);
    }
  if (listen (sockfd, BACKLOG) == -1)
    {
      perror ("listen");
      syslog (LOG_ERR, "Error on listen");
      closelog ();
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
		  syslog (LOG_ERR, "Error on accept");
		  continue;
		}
	      for (i = 0; i < MAXCLIENTS; i++)
		{
		  if (clients[i] == 0)
		    {
		      clients[i] = new_fd;
		      clients_t[i] = getMicrotime ();
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
		  strcpy (clients_ip[i], inet_ntoa (their_addr.sin_addr));
		  syslog (LOG_NOTICE, "Connexion received from %s (slot %i) ",
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
		      syslog (LOG_NOTICE, "Connexion lost from slot %i", i);
		      close (clients[i]);
		      clients[i] = 0;
		    }
		  else
		    {
		      buffer[numbytes] = '\0';
		      printf ("Received from slot %i : %s", i, buffer);
		      syslog (LOG_NOTICE, "Received from slot %i : %s", i,
			      buffer);
		      if (strncmp (buffer, "POSTGRES", 6) == 0)
			{
			  conn = PQconnectdb (conninfo);
			  if (PQstatus (conn) != CONNECTION_OK)
			    {
			      fprintf (stderr,
				       "Connection to database failed: %s",
				       PQerrorMessage (conn));
			      send (new_fd, "\nDB KO\n", 7, MSG_NOSIGNAL);
			      syslog (LOG_ALERT,
				      "Connection to database failed: %s",
				      PQerrorMessage (conn));
			    }
			  else
			    {
			      send (new_fd, "\nDB OK\n", 7, MSG_NOSIGNAL);
			    }
			  PQfinish (conn);
			}
		      if (strncmp (buffer, "DBINSERT", 6) == 0)
			{
			  conn = PQconnectdb (conninfo);
			  if (PQstatus (conn) != CONNECTION_OK)
			    {
			      fprintf (stderr,
				       "Connection to database failed: %s",
				       PQerrorMessage (conn));
			      send (new_fd, "\nDBINSERT KO (1)\n", 16,
				    MSG_NOSIGNAL);
			      syslog (LOG_ALERT,
				      "Connection to database failed: %s",
				      PQerrorMessage (conn));
			    }
			  else
			    {
			      char query_string[256];
			      send (new_fd, "\nDBINSERT OK\n", 7,
				    MSG_NOSIGNAL);
			      sprintf (query_string,
				       "INSERT INTO STATISTIQUE (CLIENT,SERVER,TEMPS_MILLI,TEMPS_CUR) VALUES ('%s','%s','%ld','%ld')",
				       clients_ip[i], localip,
				       getMicrotime () - clients_t[i],
				       time (0));
			      res = PQexec (conn, query_string);
			      if (PQresultStatus (res) != PGRES_COMMAND_OK)
				{
				  send (new_fd, "\nDBINSERT KO (2)\n", 16,
					MSG_NOSIGNAL);
				  send (new_fd, query_string, strlen(query_string), MSG_NOSIGNAL);
				  send (new_fd, "\n", 1, MSG_NOSIGNAL);
				  PQclear (res);
				}
			      else
				{
				  send (new_fd, "\nDBINSERT OK (1)\n", 16,
					MSG_NOSIGNAL);
				  PQclear (res);
				}
			    }
			  PQfinish (conn);
			}
		      if ((strncmp (buffer, "QUIT", 4) == 0))
			{
			  printf ("Connexion QUIT from slot %i \n", i);
			  syslog (LOG_NOTICE,
				  "End of cnx : QUIT (slot n°%i)", i);
			  close (clients[i]);
			  clients[i] = 0;
			}
		      if ((strncmp (buffer, "TIME", 4) == 0))
			{
			  printf ("TIME from slot %i : %ld ms \n", i,
				  getMicrotime () - clients_t[i]);
			}
		      if ((strncmp (buffer, "EXIT", 4) == 0))
			{
			  printf ("Connexion EXIT from slot %i \n", i);
			  syslog (LOG_NOTICE,
				  "End of cnx : EXIT (slot n°%i)", i);
			  close (clients[i]);
			  clients[i] = 0;
			}
		      if ((strncmp (buffer, "CLOSE", 5) == 0))
			{
			  printf ("Connexion CLOSE from slot %i \n", i);
			  syslog (LOG_NOTICE,
				  "End of cnx : CLOSE (slot n°%i)", i);
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
		      if ((strncmp (buffer, "CLIENT", 6) == 0))
			{
			  send (new_fd, "\n", 1, MSG_NOSIGNAL);
			  send (new_fd, clients_ip[i], strlen (clients_ip[i]),
				MSG_NOSIGNAL);
			  send (new_fd, "\n", 1, MSG_NOSIGNAL);
			}
		      if ((strncmp (buffer, "DBCNX", 2) == 0))
			{
			  send (new_fd, "\n", 1, MSG_NOSIGNAL);
			  send (new_fd, conninfo, strlen (conninfo),
				MSG_NOSIGNAL);
			  send (new_fd, "\n", 1, MSG_NOSIGNAL);
			}
		    }
		}
	    }
	}
      else
	{
	  perror ("SELECT");
	  syslog (LOG_ERR, "Error on select");
	  continue;
	}
    }
  return 0;
}
