8ball: tty.c tty.h cpu.c cpu.h 8ball.c linenoise.c linenoise.h rimloader.h console.c console.h
	$(CC) -Wall -W -g -o 8ball tty.c cpu.c 8ball.c linenoise.c console.c -D HOST_BUILD

clean:
	rm -f 8ball.o linenoise.o 8ball
