CC = gcc
CFLAGS += -Wall
#CFLAGS += -Wall -DVER=\"HEHE-SEF1.0\"
OBJS = mqttmain.o tools.o getlanhost.o function.o md5.o

LIBS = -luci -ljson-c -lpthread -lmosquitto

all: clean mbox 

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $< $(LIBS) 

mbox: $(OBJS)
	$(CC) -o $@ $(CFLAGS) $(OBJS) $(LIBS)


clean:
	rm -f  *.o mbox
