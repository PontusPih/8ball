#include "tty.h"
#include "cpu.h"
#include "8ball.h"

// TTY registers
short tty_kb_buf = 0;
short tty_kb_flag = 0;
short tty_tp_buf = 0;
short tty_tp_flag = 0;
short tty_dcr = 0; // device control register

void tty_reset(){
  tty_kb_buf = 0;
  tty_kb_flag = 0;
  tty_tp_buf = 0;
  tty_tp_flag = 0;
  tty_dcr = TTY_IE_MASK;
}

void tty_process(){
  // TTY and console handling:
  // If keyboard flag is not set, try to read one char.
  if( !tty_kb_flag ) {
    tty_kb_buf = read_tty_byte();
    tty_kb_flag = 1;
    if( tty_dcr & TTY_IE_MASK ){
      cpu_raise_interrupt(TTY_INTR_FLAG);
    }
  }
}
