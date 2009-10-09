BIN = bot
SRCS= main.c
OBJS= $(SRCS:.c=.o)

CC= gcc
CCFLAGS= -Wall -Iinclude -march=native -mtune=native -O2 -pipe
LD= gcc
LDFLAGS= $(LIBS)

all: $(BIN)

.c.o:
	$(CC) $(CCFLAGS) $(LIBS) -o $@ -c $<

$(BIN): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

clean:
	-rm -f *.o $(BIN)

.PHONY: all clean
