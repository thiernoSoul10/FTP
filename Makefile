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
PROGS = ftpserveri ftpclient

# Exclure les fichiers contenant main
COMMON_OBJS = $(filter-out ftpserveri.o ftpclient.o,$(OBJS))

all: $(PROGS)

ftpserveri: ftpserveri.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LIBS)

ftpclient: ftpclient.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLDIR) -c -o $@ $<

clean:
	rm -f $(PROGS) *.o