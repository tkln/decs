CFLAGS+=-std=c99 -g
LDFLAGS+=-lSDL2
OBJS=decs.o game.o

all: game

game: $(OBJS)

clean:
	rm -f $(OBJS) game
