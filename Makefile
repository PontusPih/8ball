8ball: tty.c tty.h cpu.c cpu.h 8ball.c 8ball.h linenoise.c linenoise.h rimloader.h
	$(CC) -Wall -W -g -o 8ball tty.c cpu.c 8ball.c linenoise.c

clean:
	rm -f 8ball.o linenoise.o 8ball
