OBJS=microscope.o
BIN=microscope
CFLAGS=-Wall -Werror -ggdb3
LDFLAGS=-pthread -ggdb3
LIBS=-lSDL2 -lSDL2_image 

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $< $(LIBS)

install: $(BIN)
	install -m 755 $(BIN) /usr/bin

clean:
	rm -f $(BIN) $(OBJS)
