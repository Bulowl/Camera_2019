#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <signal.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#include <assert.h>
#include <getopt.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/videodev2.h>
#include <jpeglib.h>
#include <libv4l2.h>
#include <stdint.h>
#include <inttypes.h>

#include "config.h"
#include "yuv.h"



#define MAXNAME 10
#define MAXTEXT 100
#define LEN 3 

//display who sends data and data sent
int print_msg(char *talker, char * chat) {
	int choice = chat[0];
	fputs(talker, stdout);
	fputs(" choice: ", stdout);
	fputs(chat, stdout); 
	fflush(stdout);
	return choice;
}

//affect choice
int get_msg(char * chat) {
	int choice = chat[0];
	return choice;
}

//display who connects
void read_header(int sock, char * username) {
	int loglen ;
	read(sock, &loglen, 1);
	read(sock, username, loglen);
}

int main(int argc, char * argv[])
{
	int socket_RV, socket_service;
	int pidSon;
	int port = 6112; //default port
	char name[30];
	char commandWrite[80];
	char buffer[75];
	struct sockaddr_in adr, address;
	socklen_t lgaddress; //sizeof(struct sockadr_in);
	const char *moves[] = { "Rock", "Paper", "Scissor" };
	const char *winner[] = { "DRAW", "WIN.", "LOSS" };
	
  
//display how to use commands if error
  if (argc!=2)
  {
    fprintf(stderr,"Use : %s port-number", argv[0]);
    exit(1);
  }
//define port
  port = atoi(argv[1]);
//handle error on socket
  if ((socket_RV=socket(AF_INET, SOCK_STREAM, 0)) ==-1)
  {
    perror("socket error");
    exit(1);
  }
  
//if name not defined
  if (gethostname(name, 30)==-1)
  {
    perror("Who am I?");
    exit(1);
  }
 
  printf("User: %s - %d; Machine: %s\n", getlogin(), geteuid(), name);

//socket varibales
  adr.sin_family=AF_INET;
  adr.sin_port=htons(port);
  adr.sin_addr.s_addr = htonl(INADDR_ANY);

//error on bind
  if (bind(socket_RV, (struct sockaddr *) &adr, sizeof(adr))==-1)
  {
    perror("bind");
    exit(1);
  }

//error on listen
  if (listen(socket_RV,1)==-1)
  {
    perror("listen");
    exit(1);
  }
  //accept socket connection
  socket_service=accept(socket_RV, (struct sockaddr *)&address, &lgaddress);
  close(socket_RV);

  char c;
  char *talker = (char*)malloc(MAXNAME);
  char *chat =  (char*)malloc(MAXTEXT);
  char *begchat = chat;
  
  switch(pidSon=fork()) {
  case -1:
    perror("fork");
    exit(1);
  case 0:
	
    read_header(socket_service, talker);
    printf("%s is connected.\n", talker);
    do
    {
		//get data from user
		c = EOF;
		read(socket_service, &c, 1);
		*chat = c;
		chat++;
		if (c == '\n' || c == EOF)
		{
			*chat = '\0';
			chat = begchat;
			print_msg(talker, chat);
			int choice = get_msg(chat);
			
			//if value is wrong
			if ((choice!='p') && (choice!='q')) //p = take picture, q = quit
			{
				printf("Wrong value chosen by %s.\n", talker);
				snprintf(buffer, sizeof(buffer),"Please enter a correct value: s, r or p.\n");
				write(socket_service, &buffer, strlen(buffer)+1);
				snprintf(buffer, sizeof(buffer),"If you want to quit, press q.\n");
				write(socket_service, &buffer, strlen(buffer)+1);
			}
	
			//game 
			else {		
				if (choice=='p') {choice = 1;}
		
				//compute cpu choice
				/*
				int cpu_action;
				srand(time(NULL)); 
				cpu_action = rand() % 3;
				*/
				//Take photo
				deviceOpen();
				deviceInit();

				// start capturing
				captureStart();

				// process frames
				mainLoop();

				// stop capturing
				captureStop();

				// close device
				deviceUninit();
				deviceClose();


		
				//display user result on server
				printf("%s chose %s; CPU chose %s. %s\n", talker,
					moves[choice], moves[cpu_action],
						winner[(cpu_action - choice + 3) % 3]);
					
				//send result to client
				snprintf(buffer, sizeof(buffer),"You chose %s; CPU chose %s. %s\n",
					moves[choice], moves[cpu_action],
						winner[(cpu_action - choice + 3) % 3]);
				write(socket_service, &buffer, strlen(buffer)+1);
				snprintf(buffer, sizeof(buffer), "Play again ? Press q to quit.\n");
				write(socket_service, &buffer, strlen(buffer)+1);
			}
		}
    }
    //waiting for interruption
    while (c!=EOF);
    fprintf(stderr,"Server: son ended.\n");
    break;
    
  default:
    while (c!=EOF); 

    kill(pidSon, SIGTERM);
    fprintf(stderr,"Server: main ended.\n");
  }
  return 0;
}
