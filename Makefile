SRCS=main.c common.c hottublogic.c  logfile.c TBH_LED7Backpack.c uithread.c mailit.c listener.c thingspeak.c

CFLAGS=`xml2-config --cflags`

LIBS=`xml2-config --libs`

LDFLAGS = -L/usr/local/lib -L/usr/local/ -L/usr/lib/arm-linux-gnueabihf -lwiringPi -lpthread -lrt -lcurl
# -lcurl 

OBJS=$(SRCS:.c=.o)

CC=gcc -O0 -g 

LD=gcc

all: hottub

hottub: $(OBJS)
	$(LD) $(CFLAGS) $(LIBS) -o hottub $(OBJS) $(LDFLAGS)  -D_REENTRANT
	
clean:
	rm -f $(OBJS) hottub core 

