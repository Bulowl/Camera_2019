#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#define MAXTEXT 100

/*write username in socket to see who is connected 
(if more than one client is connected)*/
void write_header(int sock, char * username) {
  if (username!=NULL) {
		int loglen = strlen(username);
		write(sock, &loglen, 1);
		write(sock, username, loglen);
	}else {
		int loglen = 8;
		write(sock, &loglen, 1);
		write(sock, "RPI3", loglen);
	}
}

/*
void sig_handler(int signo, char c,int sock)
{
  if (signo == SIGINT)
    printf("saluuuuuuuuuuuuuuuuut");
    // envoi du message de fin de connexion
    c='q';
    write(sock, &c, 1);
    
    // fermeture
	shutdown(sock,2);
    close(sock);
}
*/

void handler(int theSignal)
{
  printf("CTRL-C\n");
  // ecriture
  //char mess='p';
  //write(sock, &mess, 1);
  
  exit(1);
}

int main(int argc, char * argv[])
{
  int sock;
  int port = 6112;
  int pidSon;
  char *username;
  struct hostent * host;
  struct sockaddr_in adr;

  // signal
  struct sigaction prepaSignal;
  prepaSignal.sa_handler = &handler;
  prepaSignal.sa_flags = 0;
  sigemptyset(&prepaSignal.sa_mask);
  sigaction(SIGINT, &prepaSignal,0);

//if user entered wrong commands
  if (argc!=3)
  {
    fprintf(stderr,"Use : %s machine port-number\n", argv[0]);
    exit(1);
  }

//handle error on socket
  if ((sock=socket(AF_INET, SOCK_STREAM, 0)) ==-1)
  {
    perror("socket error");
    exit(1);
  }
  
//get hostname
  host=gethostbyname(argv[1]);
  port=atoi(argv[2]);
  if (getlogin()!=NULL) {
		username = getlogin();
	}else {
		username = "RPI3";
	}
  printf("User: %s - %d; Machine: %s\n", username, geteuid(), argv[1]);

//define variables for socket
  adr.sin_family=AF_INET;
  adr.sin_port=htons(port);
  bcopy(host->h_addr, &adr.sin_addr.s_addr, host->h_length);

//handle error on connect
  if (connect(sock, (struct sockaddr *)&adr, sizeof(adr))==-1)
  {
    perror("connect");
    exit(1);
  }

//fork process
  char c;
  switch(pidSon=fork()) {

//error on fork
  case -1:
    perror("fork");
    exit(1);
    
  case 0:
	//read incomming data from socket
    do
    {
		c = EOF;
		read(sock, &c, 1);
		putchar(c);
    }
    //waiting for ending command
    while (c!=EOF);
    fprintf(stderr,"Client: son ended.\n");
    break;
    
  default:
    /* First message sends the name of the user */
    write_header(sock, username);
    
    printf("Write p to take a picture: \n");
    printf("Enter 'q' to quit. \n");
    do
    {
		//get user input
		c=getchar();
		write(sock, &c, 1);
	}
	//interruption on command
    while ((c!=EOF) && (c!='q'));
    
    printf("exit phase:\n");
    shutdown(sock,2);
    close(sock);
    kill(pidSon,SIGTERM);
    fprintf(stderr,"Client: main ended.\n");
  }
  return 0;
}
