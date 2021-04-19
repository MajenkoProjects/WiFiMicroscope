OBJS=microscope.o
BIN=microscope
CFLAGS=-Wall -Werror
LDFLAGS=
LIBS=-lSDL2 -lSDL2_image

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $< $(LIBS)
