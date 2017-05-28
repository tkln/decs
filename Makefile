all: game

decs.o: decs.c
game.o: game.c
game: decs.o game.o
