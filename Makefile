CFLAGS+=-std=c99 -g
LDFLAGS+=-lSDL2 -lm
OBJS=decs.o game.o perf.o

all: game

game: $(OBJS)

clean:
	rm -f $(OBJS) game
