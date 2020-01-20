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


static void deviceOpen(void)
{
	struct stat st;

	// stat file
	if (-1 == stat(deviceName, &st)) {
		fprintf(stderr, "Cannot identify '%s': %d, %s\n", deviceName, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	// check if its device
	if (!S_ISCHR(st.st_mode)) {
		fprintf(stderr, "%s is no device\n", deviceName);
		exit(EXIT_FAILURE);
	}

	// open device
	fd = v4l2_open(deviceName, O_RDWR /* required */ | O_NONBLOCK, 0);

	// check if opening was successfull
	if (-1 == fd) {
		fprintf(stderr, "Cannot open '%s': %d, %s\n", deviceName, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
}


/**
	initialize device
*/
static void deviceInit(void)
{
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	struct v4l2_streamparm frameint;
	unsigned int min;

	if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
		if (EINVAL == errno) {
			fprintf(stderr, "%s is no V4L2 device\n",deviceName);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, "%s is no video capture device\n",deviceName);
		exit(EXIT_FAILURE);
	}

	switch (io) {
#ifdef IO_READ
		case IO_METHOD_READ:
			if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
				fprintf(stderr, "%s does not support read i/o\n",deviceName);
				exit(EXIT_FAILURE);
			}
			break;
#endif

#ifdef IO_MMAP
		case IO_METHOD_MMAP:
#endif
#ifdef IO_USERPTR
		case IO_METHOD_USERPTR:
#endif
#if defined(IO_MMAP) || defined(IO_USERPTR)
      			if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
				fprintf(stderr, "%s does not support streaming i/o\n",deviceName);
				exit(EXIT_FAILURE);
			}
			break;
#endif
	}

	/* Select video input, video standard and tune here. */
	CLEAR(cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect; /* reset to default */

		if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
				case EINVAL:
					/* Cropping not supported. */
					break;
				default:
					/* Errors ignored. */
					break;
			}
		}
	} else {
		/* Errors ignored. */
	}

	CLEAR(fmt);

	// v4l2_format
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = width;
	fmt.fmt.pix.height = height;
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;

	if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
		errno_exit("VIDIOC_S_FMT");

	if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUV420) {
		fprintf(stderr,"Libv4l didn't accept YUV420 format. Can't proceed.\n");
		exit(EXIT_FAILURE);
	}

	/* Note VIDIOC_S_FMT may change width and height. */
	if (width != fmt.fmt.pix.width) {
		width = fmt.fmt.pix.width;
		fprintf(stderr,"Image width set to %i by device %s.\n", width, deviceName);
	}

	if (height != fmt.fmt.pix.height) {
		height = fmt.fmt.pix.height;
		fprintf(stderr,"Image height set to %i by device %s.\n", height, deviceName);
	}
	
  /* If the user has set the fps to -1, don't try to set the frame interval */
  if (fps != -1)
  {
    CLEAR(frameint);
    
    /* Attempt to set the frame interval. */
    frameint.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    frameint.parm.capture.timeperframe.numerator = 1;
    frameint.parm.capture.timeperframe.denominator = fps;
    if (-1 == xioctl(fd, VIDIOC_S_PARM, &frameint))
      fprintf(stderr,"Unable to set frame interval.\n");
  }

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	switch (io) {
#ifdef IO_READ
		case IO_METHOD_READ:
			readInit(fmt.fmt.pix.sizeimage);
			break;
#endif

#ifdef IO_MMAP
		case IO_METHOD_MMAP:
			mmapInit();
			break;
#endif

#ifdef IO_USERPTR
		case IO_METHOD_USERPTR:
			userptrInit(fmt.fmt.pix.sizeimage);
			break;
#endif
	}
}



/**
	mainloop: read frames and process them
*/
static void mainLoop(void)
{	
	int count;
	unsigned int numberOfTimeouts;

	numberOfTimeouts = 0;
	count = 3;

	while (count-- > 0) {
		for (;;) {
			fd_set fds;
			struct timeval tv;
			int r;

			FD_ZERO(&fds);
			FD_SET(fd, &fds);

			/* Timeout. */
			tv.tv_sec = 1;
			tv.tv_usec = 0;

			r = select(fd + 1, &fds, NULL, NULL, &tv);

			if (-1 == r) {
				if (EINTR == errno)
					continue;

				errno_exit("select");
			}

			if (0 == r) {
				if (numberOfTimeouts <= 0) {
					count++;
				} else {
					fprintf(stderr, "select timeout\n");
					exit(EXIT_FAILURE);
				}
			}
			if(continuous == 1) {
				count = 3;
			}

			if (frameRead())
				break;

			/* EAGAIN - continue select loop. */
		}
	}
}



/**
	stop capturing
*/
static void captureStop(void)
{
	enum v4l2_buf_type type;

	switch (io) {
#ifdef IO_READ
		case IO_METHOD_READ:
			/* Nothing to do. */
			break;
#endif

#ifdef IO_MMAP
		case IO_METHOD_MMAP:
#endif
#ifdef IO_USERPTR
		case IO_METHOD_USERPTR:
#endif
#if defined(IO_MMAP) || defined(IO_USERPTR)
			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

			if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
			errno_exit("VIDIOC_STREAMOFF");

			break;
#endif
	}
}

/**
  start capturing
*/
static void captureStart(void)
{
	unsigned int i;
	enum v4l2_buf_type type;

	switch (io) {
#ifdef IO_READ
		case IO_METHOD_READ:
			/* Nothing to do. */
			break;
#endif

#ifdef IO_MMAP
		case IO_METHOD_MMAP:
			for (i = 0; i < n_buffers; ++i) {
				struct v4l2_buffer buf;

				CLEAR(buf);

				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_MMAP;
				buf.index = i;

				if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
					errno_exit("VIDIOC_QBUF");
				}

			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

			if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
				errno_exit("VIDIOC_STREAMON");

			break;
#endif

#ifdef IO_USERPTR
		case IO_METHOD_USERPTR:
			for (i = 0; i < n_buffers; ++i) {
				struct v4l2_buffer buf;

			CLEAR (buf);

			buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory = V4L2_MEMORY_USERPTR;
			buf.index = i;
			buf.m.userptr = (unsigned long) buffers[i].start;
			buf.length = buffers[i].length;

			if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
				errno_exit("VIDIOC_QBUF");
			}

			type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

			if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
				errno_exit("VIDIOC_STREAMON");

			break;
#endif
	}
}


static void deviceUninit(void)
{
	unsigned int i;

	switch (io) {
#ifdef IO_READ
		case IO_METHOD_READ:
			free(buffers[0].start);
			break;
#endif

#ifdef IO_MMAP
		case IO_METHOD_MMAP:
			for (i = 0; i < n_buffers; ++i)
				if (-1 == v4l2_munmap(buffers[i].start, buffers[i].length))
					errno_exit("munmap");
			break;
#endif

#ifdef IO_USERPTR
		case IO_METHOD_USERPTR:
			for (i = 0; i < n_buffers; ++i)
				free(buffers[i].start);
			break;
#endif
	}

	free(buffers);
}

/**
	close device
*/
static void deviceClose(void)
{
	if (-1 == v4l2_close(fd))
		errno_exit("close");

	fd = -1;
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
