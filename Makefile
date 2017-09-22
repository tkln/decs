CFLAGS+=-std=c99 -g
LDFLAGS+=-lSDL2 -lSDL2_ttf -lm
OBJS=decs.o game.o perf.o ttf.o

all: game

game: $(OBJS)

clean:
	rm -f $(OBJS) game
