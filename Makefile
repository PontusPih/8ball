all: 8ball

8ball: tty.c tty.h cpu.c cpu.h 8ball.c linenoise.c linenoise.h rimloader.h console.c console.h machine.c machine.h rx8.c rx8.h
	$(CC) -Wall -W -g -o 8ball tty.c cpu.c 8ball.c console.c machine.c linenoise.c rx8.c -DSERVER_BUILD -fmax-errors=5

8con: 8ball.c linenoise.c console.h console.c machine.c machine.h serial_com.c serial_com.h
	$(CC) -Wall -W -g -o 8con 8ball.c linenoise.c console.c machine.c serial_com.c -DPTY_CLI -fmax-errors=1

8srv: 8ball.c machine.c machine.h tty.c tty.h cpu.c cpu.h rimloader.h serial_com.c serial_com.h rx8.c rx8.h
	$(CC) -Wall -W -g -o 8srv 8ball.c machine.c tty.c cpu.c serial_com.c -DPTY_SRV -fmax-errors=1

clean:
	rm -f 8ball.o linenoise.o 8ball 8con
