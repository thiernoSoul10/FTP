.PHONY: all clean

.SUFFIXES:

CC = gcc
CFLAGS = -Wall -Werror
LIBS += -lpthread
INCLDIR = -I.

# Tous les fichiers .c
SRCS = $(wildcard *.c)

# Tous les objets
OBJS = $(SRCS:.c=.o)

# Exécutables
PROGS = ftpserveri ftpclient ftpmaster ftpslave

# Exclure les fichiers contenant main
COMMON_OBJS = $(filter-out ftpserveri.o ftpclient.o ftpmaster.o ftpslave.o,$(OBJS))

all: $(PROGS)

ftpserveri: ftpserveri.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LIBS)

ftpclient: ftpclient.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LIBS)

ftpmaster: ftpmaster.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LIBS)

ftpslave: ftpslave.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLDIR) -c -o $@ $<

clean:
	rm -f $(PROGS) *.o