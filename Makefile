all: 8ball

8ball: tty.c tty.h cpu.c cpu.h 8ball.c linenoise.c linenoise.h rimloader.h console.c console.h machine.c machine.h
	$(CC) -Wall -W -g -o 8ball tty.c cpu.c 8ball.c console.c machine.c linenoise.c -DSERVER_BUILD -fmax-errors=5 -Wno-unused-parameter

8con: 8ball.c linenoise.c console.h console.c machine.c machine.h serial_com.c serial_com.h
	$(CC) -Wall -W -g -o 8con 8ball.c linenoise.c console.c machine.c serial_com.c -DPTY_CLI -fmax-errors=1

8srv: 8ball.c machine.c machine.h tty.c tty.h cpu.c cpu.h rimloader.h serial_com.c serial_com.h
	$(CC) -Wall -W -g -o 8srv 8ball.c machine.c tty.c cpu.c serial_com.c -DPTY_SRV -fmax-errors=1

presentation:
	a2ps -r -1 -B presentation.txt -o presentation.ps
	ps2pdf presentation.ps presentation.pdf
	pdfseparate presentation.pdf %d_presentation.pdf
	pdfunite 01_straigh8.pdf 02_pants.pdf 03_block.pdf 04_d0ab.pdf 05_state.pdf 06_fetch.pdf 1_presentation.pdf 2_presentation.pdf 3_presentation.pdf 10_defer.pdf 11_execute.pdf 12_d0bb.pdf 13_d0bb.pdf 4_presentation.pdf pres.pdf

clean:
	rm -f 8ball.o linenoise.o 8ball 8con
