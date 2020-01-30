/*
  Server finds images in directory.
  Creates entries into catalog.csv.
  Waits for clients.
  Responds to requests.
*/


/****Necessary includes****/
#include "md5sum.h"
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/sendfile.h>
#include <fcntl.h>


/*****Required global variables****/
char* logpath;
char* jpeg_array[200];

int jpeg_count = 0;

char* file_array[200];
char* file_sizes[200];
int file_count = 0;

/**********Define for GPIO (led control*************/
#define IN 0
#define OUT 1

#define LOW  0
#define HIGH 1

#define PINCONNECT  17 /* P1-18 */
#define PINTRANSFER 18  /* P1-07 */

/**********GPIO Function************/

static int
GPIOExport(int pin)
{
#define BUFFER_MAX 3
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;

	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open export for writing!\n");
		return(-1);
	}

	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}

static int
GPIOUnexport(int pin)
{
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;

	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open unexport for writing!\n");
		return(-1);
	}

	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}

static int
GPIODirection(int pin, int dir)
{
	static const char s_directions_str[]  = "in\0out";

#define DIRECTION_MAX 35
	char path[DIRECTION_MAX];
	int fd;

	snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio direction for writing!\n");
		return(-1);
	}

	if (-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
		fprintf(stderr, "Failed to set direction!\n");
		return(-1);
	}

	close(fd);
	return(0);
}

static int
GPIORead(int pin)
{
#define VALUE_MAX 30
	char path[VALUE_MAX];
	char value_str[3];
	int fd;

	snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_RDONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for reading!\n");
		return(-1);
	}

	if (-1 == read(fd, value_str, 3)) {
		fprintf(stderr, "Failed to read value!\n");
		return(-1);
	}

	close(fd);

	return(atoi(value_str));
}

static int
GPIOWrite(int pin, int value)
{
	static const char s_values_str[] = "01";

	char path[VALUE_MAX];
	int fd;

	snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for writing!\n");
		return(-1);
	}

	if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
		fprintf(stderr, "Failed to write value!\n");
		return(-1);
	}

	close(fd);
	return(0);
}


//Writes file size and file data to the socket
void write_to_socket(char* file_name, int sockfd)
{
   FILE* fp;
  
   if((fp = fopen(file_name,"rb")) == NULL)
	{
      printf("Error I guess? %s\n",strerror(errno));}
       //printf("Cannot open file\n");
  
	  fseeko(fp,0,SEEK_END);
	  int sz = ftello(fp);
	  fseeko(fp,0,SEEK_SET);
	  
	  char sizeBuf[256];
	  
	  sprintf(sizeBuf,"%d",sz);

	  write(sockfd,sizeBuf,256);
	  int n;
	  while(1)
    	{
	    	unsigned char buf[256] = {0};
	        int n = fread(buf,1,256,fp);
		  
		    if(n>0)
				{
		 			write(sockfd,buf,n);
				}
      
			if(n<256)
				{
	  				if(feof(fp))
				    printf("Finished sending: %s\n",file_name);
				 	break;
				}
   		}
  	fclose(fp);

}
    
//Writes file info to the catalog.
void write_to_catalog(char* file_name, int file_size, unsigned char* sum)
{
  int i;
  
  FILE* fp;
  fp = fopen(logpath,"a");
  fprintf(fp,"%s",file_name);
  fprintf(fp,",");
  fprintf(fp,"%d",file_size);
  fprintf(fp,",");
  for(i=0;i<MD5_DIGEST_LENGTH;i++)
    {
      fprintf(fp,"%02x",sum[i]);
    }
  fprintf(fp,"%s\n"," ");
  fclose(fp);
}

int is_jpeg(char* file_name)
{
  if(strstr(file_name,"jpeg")) return 1;
  return 0;
}

//Adds files to the corresponding arrays.
void addToArray(char* file_path)
{
  if(is_jpeg(file_path))
    {
      jpeg_array[jpeg_count] = file_path;
      jpeg_count++;
    }
  //jpeg_count = 10;
  
  file_array[file_count] = file_path;
  file_count++;
}

//Recursively finds images in directories and sub-directories
int find_images(char* in_dir,char* catpath)
{
  DIR* indir;
  struct dirent* file;
  
  if((indir=opendir(in_dir)) == NULL)
    {
      perror("Unable to open target directory\n");
      exit(1);
    }
  
  while((file = readdir(indir))!=NULL)
    {
      if((file->d_name[0]) == '.') continue;
      
      if(file->d_type == DT_DIR)
		{
		  
		  char* path2 = malloc(200);
		  strcpy(path2,in_dir);
		  strcat(path2,"/");
		  strcat(path2,file->d_name);
		  find_images(path2,catpath);
		}
      
      if(is_jpeg(file->d_name))
		{
		  char* file_path = malloc(200);
		  strcpy(file_path,in_dir);
		  strcat(file_path,"/");
		  strcat(file_path,file->d_name);
		 
		  unsigned char sum[MD5_DIGEST_LENGTH];
		  int i;
		  addToArray(file_path);
		  md5sum(file_path,sum);
		  
		  struct stat st = {0};
		  
		  stat(file_path,&st);
		  
		  write_to_catalog(file->d_name,(int)st.st_size,sum);
	
		}
	}
  return 0;}


int main(int argc, char* argv[])
{
  
	if(argc!=3)
	{
		perror("Incorrect number of arguments\n");
		perror("Usage: ./server <port_number> <directory>\n");
		exit(1);
	}

	/*********** Define GPIO ***************/
	//Enable GPIO pins
	if (-1 == GPIOExport(PINCONNECT) || -1 == GPIOExport(PINTRANSFER))
		return(1);

	//Set GPIO directions
	if (-1 == GPIODirection(PINCONNECT, OUT) || -1 == GPIODirection(PINCONNECT, OUT))
		return(2);

	/**************************************/

	logpath = malloc(200);


	strcpy(logpath,argv[2]);
	strcat(logpath,"/");
	strcat(logpath,"catalog.csv");


	struct stat st = {0};

	if(stat(logpath,&st) !=-1)
	{
		perror("Catalog file exists. Deleting it.\n");
		remove(logpath);
		FILE* fp;
		fp = fopen(logpath,"a+");
		perror("catalog.csv created\n");
		fclose(fp);
	}

	else
	{
	  
	  FILE* fp;
	  fp = fopen(logpath,"a+");
	  perror("catalog.csv created\n");
	  fclose(fp);
	}


	FILE* fp;
	fp = fopen(logpath,"a+");

	fprintf(fp,"File name,Size,Sum\n");
	fclose(fp);
	int i;
	int result;

	if((result=find_images(argv[2],logpath))!=0)
	printf("Error finding images\n");

	int listenfd;
	int confd;

	struct sockaddr_in serv_addr;

	char sendBuff[1025];
	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	memset(&serv_addr,'0',sizeof(serv_addr));
	memset(sendBuff,'0',sizeof(sendBuff));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(argv[1]));

	bind(listenfd,(struct sockaddr*)&serv_addr, sizeof(serv_addr));

	if(listen(listenfd,10) == -1)
	{
		perror("Could not listen\n");
		return -1;
	}

	while(1)
	{

		printf("Listening for clients...\n");

		confd = accept(listenfd, (struct sockaddr*)NULL, NULL);

		int n;

		char buf[10];

		printf("accepted a connection\n");

		//PINCONNECT ON
		if (-1 == GPIOWrite(PINCONNECT, 15))
			return(3);
      
		write_to_socket(logpath,confd);

		read(confd,buf,11);


		if(is_jpeg(buf))
	    {
			printf("Client in passive mode. Requesting %s files:\n",buf);
	      
			if(is_jpeg(buf))
			{


				if (-1 == GPIOWrite(PINTRANSFER, 15))
					return(4);

				for(i=0;i<jpeg_count;i++){

					char buffer[4];
					read(confd,buffer,5);
				      
				    	if(strstr(buffer,"ready"))
							write_to_socket(jpeg_array[i],confd);

				}
			}
	    }
	     
      
      //sleep(1);
      
		return 0;
	}

	close(confd);

	return 0;
}
