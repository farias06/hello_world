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
#include<signal.h>
#include<string.h>

/* Constantes */

#define MYPORT 		80
#define BACKLOG 	5
#define MAXCLIENTS 	5
#define MAXDATASIZE 	100
#define MYPATH 			"/app/server/"
#define MYPATH_HAPROXY		"/app/server/haproxy"
#define MYPATH_CONF		"/app/server/conf"
#define MYPATH_DB               "/app/server/db"
#define MYPATH_STATUS		"/app/server/status"
#define MYPATH_DEBUG		"/app/server/debug"
#define MYPATH_LOAD		"/app/server/load"
#define MYPATH_SYSLOG           "/app/server/syslog"
#define MYPATH_MAX              "/app/server/max"
#define MYPATH_VERSION          "/app/server/version"
#define MYPATH_TIME		"/app/server/time"
#define MYPATH_CA               "/app/server/client_accepted"
#define MYPATH_CR               "/app/server/client_refused"
#define MYPATH_ERROR            "/app/server/error"

/* Global */
int value_debug = 1;
int value_syslog = 1;

long
getMicrotime ()
{
  struct timeval currentTime;
  gettimeofday (&currentTime, NULL);
  return currentTime.tv_sec * (int) 1e6 + currentTime.tv_usec;
}

int
putGeneric (char filename[], char status[], int value_debug, int value_syslog)
{
  FILE *file = NULL;
  if ((file = fopen (filename, "w")) == NULL)
    {
      if (value_syslog == 1)
	{
	  syslog (LOG_ERR, "Erreur dans l'ouverture de %s", filename);
	}
      return -1;
    }
  else
    {
      fprintf (file, "%s", status);
      fclose (file);
    }
  return 0;
}

int
putGeneric2 (char filename[], int status, int value_debug, int value_syslog)
{
  FILE *file = NULL;
  if ((file = fopen (filename, "w")) == NULL)
    {
      if (value_syslog == 1)
	{
	  syslog (LOG_ERR, "Erreur dans l'ouverture de %s", filename);
	}
      return -1;
    }
  else
    {
      fprintf (file, "%d", status);
      fclose (file);
    }
  return 0;
}

int
putGeneric3 (char filename[], unsigned long status, int value_debug,
	     int value_syslog)
{
  FILE *file = NULL;
  if ((file = fopen (filename, "w")) == NULL)
    {
      if (value_syslog == 1)
	{
	  syslog (LOG_ERR, "Erreur dans l'ouverture de %s", filename);
	}
      return -1;
    }
  else
    {
      fprintf (file, "%ld", status);
      fclose (file);
    }
  return 0;
}

int
getGeneric2 (char filename[], int value_debug, int value_syslog)
{
  int status = -1;
  char string[20];
  FILE *file = NULL;
  if ((file = fopen (filename, "r")) == NULL)
    {
      if (value_syslog == 1)
	{
	  syslog (LOG_ERR, "Erreur dans l'ouverture de %s", filename);
	}
      return -1;
    }
  else
    {
      fgets (string, sizeof (string), file);
      status = atoi (string);
      fclose (file);
    }
  return status;
}

void
fin_du_programme (void)
{
  if (value_debug == 1)
    fprintf (stderr, "Fin du programme\n");
  if (value_syslog == 1)
    syslog (LOG_ERR, "Fin du programme");
  closelog ();
  putGeneric (MYPATH_STATUS, "KO", value_debug, value_syslog);
  putGeneric2 (MYPATH_ERROR, 1, value_debug, value_syslog);
  printf ("[ END ]\n");
}

struct sigaction old_action;

void
fin_du_programme2 (int sig_no)
{
  if (value_debug == 1)
    fprintf (stderr, "Fin du programme\n");
  if (value_syslog == 1)
    syslog (LOG_ERR, "Fin du programme");
  closelog ();
  putGeneric (MYPATH_STATUS, "KO", value_debug, value_syslog);
  putGeneric2 (MYPATH_ERROR, 11, value_debug, value_syslog);
  printf ("[ END : CTRL-C]\n");
  sigaction (SIGINT, &old_action, NULL);
  kill (0, SIGINT);

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
  char haproxy[MAXDATASIZE];
  char conf[MAXDATASIZE];
  unsigned long client_accepted = 0;
  unsigned long client_refused = 0;
  struct sockaddr_in my_addr, their_addr;
  socklen_t sin_size;
  struct timeval tv;
  fd_set readfds;
  int ret = -1;
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

  putGeneric (MYPATH_STATUS, "OK", value_debug, value_syslog);
  putGeneric2 (MYPATH_SYSLOG, value_syslog, value_debug, value_syslog);
  putGeneric2 (MYPATH_DEBUG, value_debug, value_debug, value_syslog);
  putGeneric2 (MYPATH_LOAD, 0, value_debug, value_syslog);
  putGeneric2 (MYPATH_MAX, MAXCLIENTS, value_debug, value_syslog);
  putGeneric2 (MYPATH_VERSION, 8, value_debug, value_syslog);
  putGeneric3 (MYPATH_CA, client_accepted, value_debug, value_syslog);
  putGeneric3 (MYPATH_CR, client_refused, value_debug, value_syslog);

  if (atexit (fin_du_programme) != 0)
    {
      fprintf (stderr, "Unable to set exit function\n");
      syslog (LOG_ERR, "Unable to set exit function");
      closelog ();
      putGeneric (MYPATH_STATUS, "KO", value_debug, value_syslog);
      putGeneric2 (MYPATH_ERROR, 7, value_debug, value_syslog);
      exit (-1);
    }

  struct sigaction action;
  memset (&action, 0, sizeof (action));
  action.sa_handler = &fin_du_programme2;
  sigaction (SIGINT, &action, &old_action);

  f = fopen ("/proc/net/route", "r");
  while (fgets (line, 100, f))
    {
      p = strtok (line, " \t");
      c = strtok (NULL, " \t");

      if (p != NULL && c != NULL)
	{
	  if (strcmp (c, "00000000") == 0)
	    {
	      if (value_debug == 1)
		{
		  fprintf (stdout, "Default interface is : %s \n", p);
		}
	      break;
	    }
	}
    }

  if (sock < 0)
    {
      fprintf (stderr, "Socket error\n");
      syslog (LOG_ERR, "Error on socket");
      closelog ();
      putGeneric (MYPATH_STATUS, "KO", value_debug, value_syslog);
      putGeneric2 (MYPATH_ERROR, 2, value_debug, value_syslog);
      exit (-1);
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
      fprintf (stdout, "Local ip is : %s \n", localip);
    }
  else
    {
      fprintf (stdout, "Error number : %d . Error message : %s \n", errno,
	       strerror (errno));
      strcpy (localip, "Error");
    }

  close (sock);

  sprintf (haproxy, "server %s %s:%d", p, localip, MYPORT);
  putGeneric (MYPATH_HAPROXY, haproxy, value_debug, value_syslog);

  sprintf (conf, "debug = %d \nsyslog = %d \ngoogle = %s \n", value_debug,
	   value_syslog, google_dns_server);
  putGeneric (MYPATH_CONF, conf, value_debug, value_syslog);

  fprintf (stdout, "POSTGRES2_PORT_5432_TCP_ADDR : %s \n",
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

  putGeneric (MYPATH_DB, conninfo, value_debug, value_syslog);

  /* Ceinture et breteille */
  fprintf (stdout, "Stdout is descriptor %d\n", fileno (stdout));
  fprintf (stdout, "Stderr is descriptor %d\n", fileno (stderr));
  freopen ("/dev/stdout", "w", stdout);
  freopen ("/dev/stderr", "w", stderr);
  fprintf (stdout, "Stdout is descriptor %d\n", fileno (stdout));
  fprintf (stdout, "Stderr is descriptor %d\n", fileno (stderr));

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
	  fprintf (stdout, "KO : Table STATISTIQUE \n");
	  PQclear (res);
	  res =
	    PQexec (conn,
		    "CREATE TABLE STATISTIQUE ( ID serial primary key, CLIENT CHAR(20) NOT NULL, SERVER CHAR(20) NOT NULL, TEMPS_MILLI INT NOT NULL, TEMPS_CUR INT);");
	  if (PQresultStatus (res) != PGRES_COMMAND_OK)
	    {
	      fprintf (stdout, "KO : Table STATISTIQUE (CREATE) \n");
	      PQclear (res);
	    }
	  else
	    {
	      fprintf (stdout, "OK : Table STATISTIQUE (CREATE) \n");
	      PQclear (res);
	    }
	}
      else
	{
	  fprintf (stdout, "OK : Table STATISTIQUE \n");
	  PQclear (res);
	}
    }
  PQfinish (conn);

  syslog (LOG_NOTICE, "End of check of table");

  if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      fprintf (stderr, "socket error\n");
      syslog (LOG_ERR, "Error on socket");
      closelog ();
      putGeneric (MYPATH_STATUS, "KO", value_debug, value_syslog);
      putGeneric2 (MYPATH_ERROR, 3, value_debug, value_syslog);
      exit (-1);
    }
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons (MYPORT);
  my_addr.sin_addr.s_addr = INADDR_ANY;
  bzero (&(my_addr.sin_zero), 8);

  if (bind (sockfd, (struct sockaddr *) &my_addr, sizeof (struct sockaddr)) ==
      -1)
    {
      fprintf (stderr, "bind error\n");
      syslog (LOG_ERR, "Error on bind");
      closelog ();
      putGeneric (MYPATH_STATUS, "KO", value_debug, value_syslog);
      putGeneric2 (MYPATH_ERROR, 4, value_debug, value_syslog);
      exit (-1);
    }
  if (listen (sockfd, BACKLOG) == -1)
    {
      fprintf (stderr, "listen error\n");
      syslog (LOG_ERR, "Error on listen");
      closelog ();
      putGeneric (MYPATH_STATUS, "KO", value_debug, value_syslog);
      putGeneric2 (MYPATH_ERROR, 5, value_debug, value_syslog);
      exit (-1);
    }
  bzero (clients, sizeof (clients));
  highest = sockfd;
  while (1)
    {
      int nb = 0;
      int new_debug = -1;
      int new_syslog = -1;

      putGeneric2 (MYPATH_TIME, time (0), value_debug, value_syslog);

      new_debug = getGeneric2 (MYPATH_DEBUG, value_debug, value_syslog);
      new_syslog = getGeneric2 (MYPATH_SYSLOG, value_debug, value_syslog);

      if ((new_debug >= 0) && (new_debug != value_debug))
	{
	  value_debug = new_debug;
	  sprintf (conf, "debug = %d \nsyslog = %d \ngoogle = %s \n",
		   value_debug, value_syslog, google_dns_server);
	  putGeneric (MYPATH_CONF, conf, value_debug, value_syslog);
	}
      if ((new_syslog >= 0) && (new_syslog != value_syslog))
	{
	  value_syslog = new_syslog;
	  sprintf (conf, "debug = %d \nsyslog = %d \ngoogle = %s \n",
		   value_debug, value_syslog, google_dns_server);
	  putGeneric (MYPATH_CONF, conf, value_debug, value_syslog);
	}

      sin_size = sizeof (struct sockaddr_in);
      tv.tv_sec = 0;
      tv.tv_usec = 330000;
      FD_ZERO (&readfds);
      for (i = 0; i < MAXCLIENTS; i++)
	{
	  if (clients[i] != 0)
	    {
	      FD_SET (clients[i], &readfds);
	      nb++;
	    }
	}
      putGeneric2 (MYPATH_LOAD, nb, value_debug, value_syslog);
      FD_SET (sockfd, &readfds);
      if (select (highest + 1, &readfds, NULL, NULL, &tv) >= 0)
	{
	  if (FD_ISSET (sockfd, &readfds))
	    {
	      if ((new_fd =
		   accept (sockfd, (struct sockaddr *) &their_addr,
			   &sin_size)) == -1)
		{
		  if (value_debug == 1)
		    fprintf (stderr, "accept error\n");
		  if (value_syslog == 1)
		    syslog (LOG_ERR, "Error on accept");
		  continue;
		}
	      for (i = 0; i < MAXCLIENTS; i++)
		{
		  if (clients[i] == 0)
		    {
		      client_accepted++;
		      putGeneric3 (MYPATH_CA, client_accepted, value_debug,
				   value_syslog);
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
		  if (value_debug == 1)
		    fprintf (stdout, "Connexion received from %s (slot %i) ",
			     inet_ntoa (their_addr.sin_addr), i);
		  strcpy (clients_ip[i], inet_ntoa (their_addr.sin_addr));
		  if (value_syslog == 1)
		    syslog (LOG_NOTICE,
			    "Connexion received from %s (slot %i) ",
			    inet_ntoa (their_addr.sin_addr), i);
		  send (new_fd, "\nHELLO\n", 7, MSG_NOSIGNAL);
		}
	      else
		{
		  client_refused++;
		  putGeneric3 (MYPATH_CR, client_refused, value_debug,
			       value_syslog);
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
		      if (value_debug == 1)
			fprintf (stdout, "Connexion lost from slot %i", i);
		      if (value_syslog == 1)
			syslog (LOG_NOTICE, "Connexion lost from slot %i", i);
		      close (clients[i]);
		      clients[i] = 0;
		    }
		  else
		    {
		      buffer[numbytes] = '\0';
		      if (value_debug == 1)
			fprintf (stdout, "Received from slot %i : %s", i,
				 buffer);
		      if (value_syslog == 1)
			syslog (LOG_NOTICE, "Received from slot %i : %s",
				i, buffer);
		      if (strncmp (buffer, "POSTGRES", 6) == 0)
			{
			  conn = PQconnectdb (conninfo);
			  if (PQstatus (conn) != CONNECTION_OK)
			    {
			      if (value_debug == 1)
				fprintf (stderr,
					 "Connection to database failed: %s \n",
					 PQerrorMessage (conn));
			      send (new_fd, "\nDB KO\n", 7, MSG_NOSIGNAL);
			      if (value_syslog == 1)
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
				       "Connection to database failed: %s \n",
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
				  send (new_fd, query_string,
					strlen (query_string), MSG_NOSIGNAL);
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
			  if (value_debug == 1)
			    fprintf (stdout,
				     "Connexion QUIT from slot %i \n", i);
			  syslog (LOG_NOTICE,
				  "End of cnx : QUIT (slot n°%i)", i);
			  close (clients[i]);
			  clients[i] = 0;
			}
		      if ((strncmp (buffer, "TIME", 4) == 0))
			{
			  if (value_debug == 1)
			    fprintf (stdout,
				     "TIME from slot %i : %ld ms \n", i,
				     getMicrotime () - clients_t[i]);
			}
		      if ((strncmp (buffer, "EXIT", 4) == 0))
			{
			  if (value_debug == 1)
			    fprintf (stdout,
				     "Connexion EXIT from slot %i \n", i);
			  syslog (LOG_NOTICE,
				  "End of cnx : EXIT (slot n°%i)", i);
			  close (clients[i]);
			  clients[i] = 0;
			}
		      if ((strncmp (buffer, "CLOSE", 5) == 0))
			{
			  if (value_debug == 1)
			    fprintf (stdout,
				     "Connexion CLOSE from slot %i \n", i);
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
			  send (new_fd, clients_ip[i],
				strlen (clients_ip[i]), MSG_NOSIGNAL);
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
	  fprintf (stderr, "select error\n");
	  syslog (LOG_ERR, "Error on select");
	  continue;
	}
    }
  putGeneric (MYPATH_STATUS, "KO", value_debug, value_syslog);
  putGeneric2 (MYPATH_ERROR, 6, value_debug, value_syslog);
  return 0;
}
