8ball: 8ball.c linenoise.c linenoise.h binloader.h
	$(CC) -Wall -W -g -o 8ball 8ball.c linenoise.c

clean:
	rm -f 8ball.o linenoise.o 8ball
