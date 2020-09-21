all: 8ball

8ball: tty.c tty.h cpu.c cpu.h 8ball.c linenoise.c linenoise.h rimloader.h console.c console.h machine.c machine.h rx8.c rx8.h rxloader.h backend.c backend.h frontend.c frontend.h
	$(CC) -Wall -W -g -o 8ball tty.c cpu.c 8ball.c console.c machine.c backend.c frontend.c linenoise.c rx8.c -fmax-errors=1 -Werror

8con: 8ball.c frontend.c frontend.h linenoise.c console.h console.c machine.c machine.h serial_com.c serial_com.h
	$(CC) -Wall -W -g -o 8con 8ball.c frontend.c linenoise.c console.c machine.c serial_com.c -DPTY_CLI=\"8con\" -fmax-errors=1 -Werror

8srv: backend.c backend.h tty.c tty.h cpu.c cpu.h rimloader.h serial_com.c serial_com.h rx8.c rx8.h
	$(CC) -Wall -W -g -o 8srv backend.c tty.c cpu.c serial_com.c rx8.c -fmax-errors=1 -Werror -D PTY_CLI=\"8srv\"

clean:
	rm -f 8ball.o linenoise.o 8ball 8con 8srv
