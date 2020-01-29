CFLAGS += -Wall
LDFLAGS += -lcrypto

SRC := server.c 

MD5_SRC := md5sum.c
MD5_OBJ := $(MD5_SRC:.c=.o)

TARGET := $(SRC:.c=)
TARGET_OBJ := $(SRC:.c=.o)

.PHONY: all clean

all: server client

server: server.o md5sum.o
	./../output/host/usr/bin/arm-linux-gcc -o $@ $^ $(LDFLAGS)

client: client.o md5sum.o
	./../output/host/usr/bin/arm-linux-gcc -o $@ $^ $(LDFLAGS)

server.o: server.c 
	./../output/host/usr/bin/arm-linux-gcc -c $^

client.o: client.c
	./../output/host/usr/bin/arm-linux-gcc -c $^

md5sum.o: md5sum.c md5sum.h
	./../output/host/usr/bin/arm-linux-gcc -c $^

clean:
	-$(RM) $(TARGET) $(TARGET_OBJ) $(MD5_OBJ) server client
