IDIR =../include
CC=gcc
CFLAGS=-I$(IDIR) -O2

ODIR=obj
LDIR =../lib

LIBS=-lpthread

_DEPS = rsa.h aes.h hashmap.h linked_list.h server.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = rsa.o aes.o hashmap.o linked_list.o server.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

server: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)
