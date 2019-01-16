/* linie noua */
/* cliTCPIt.c - Exemplu de client TCP
   Trimite un nume la server; primeste de la server "Hello nume".
         
   Autor: Lenuta Alboaie  <adria@infoiasi.ro> (c)2009
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>

/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;

int main (int argc, char *argv[])
{
  int sd;			// descriptorul de socket
  struct sockaddr_in server;	// structura folosita pentru conectare 
  char msg[100];		// mesajul trimis
  char* buff;   // buffer de receptie mesaje server
  int n; // dimensiunea bufferului

  /* exista toate argumentele in linia de comanda? */
  if (argc != 3)
    {
      printf ("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
      return -1;
    }

  /* stabilim portul */
  port = atoi (argv[2]);

  /* cream socketul */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("Eroare la socket().\n");
      return errno;
    }

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  /* familia socket-ului */
  server.sin_family = AF_INET;
  /* adresa IP a serverului */
  server.sin_addr.s_addr = inet_addr(argv[1]);
  /* portul de conectare */
  server.sin_port = htons (port);
  
  /* ne conectam la server */
  if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
      perror ("[client]Eroare la connect().\n");
      return errno;
    }

  /* bucla citire info server si trimitere raspuns, pentru iesire raspunsul este 5 */
  while(1)
    {
      /* citirea raspunsului dat de server 
        (apel blocant pina cind serverul raspunde) */

    
      /* de verificat daca face alocare la msg */
      /* citesc de la server lungimea mesajului ce urmeaza sa fie trimis */
      if (read (sd, msg, 100) < 0)
        {
          perror ("[client]Eroare la read() de la server.\n");
          return errno;
        }

      /*  alloc memorie pentru mesaj */
      n = atoi(msg);
      buff = malloc(n+1);
      
      /* citesc mesajul de la server */
      bzero(buff, n+1);
      if (read (sd, buff, n) < 0)
        {
          perror ("[client]Eroare la read() de la server.\n");
          return errno;
        }
    
      /* afisam mesajul primit */
      printf ("%s", buff);
      fflush (stdout);
    
      /*  dealoc memorie mesaj */
      free(buff);

      /* citesc de la tastatura mesajul de trimis la server */
      bzero (msg, 100);
      read (0, msg, 100);
      
      /* daca mesajul este 5 se trimite mesajul la server, se inchide conexiunea si se iese din bucla */
      if(msg[0] ==  '5' || msg[0] == 'n')
        {
          /* inchidem conexiunea, am terminat */
          printf("Exit..\n");
          if (write (sd, msg, strlen(msg)) <= 0)
          {
            perror ("[client]Eroare la write() spre server.\n");
            return errno;
          }
          close (sd);
          break;
        }
      else
        {
          /* altfel doar se trimite mesajul catre server */
          if (write (sd, msg, 100) <= 0)
            {
              perror ("[client]Eroare la write() spre server.\n");
              return errno;
            }
        }
    } 

  return 0;

}

// 127.0.0.1 2024
