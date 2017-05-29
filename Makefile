CFLAGS+=-std=c99 -g
OBJS=decs.o game.o

all: game

game: $(OBJS)

clean:
	rm -f $(OBJS) game
