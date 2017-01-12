#include "tty.h"
#include "cpu.h"
#include "console.h"

// TTY registers
short tty_kb_buf = 0;
short tty_kb_flag = 0;
short tty_tp_buf = 0;
short tty_tp_flag = 0;
short tty_dcr = 0; // device control register

// TTY internals
char output_pending = 0;

void tty_initiate_output()
{
  output_pending = 1;
}

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
    char input, res;
    res = read_tty_byte(&input);
    if( res ) {
      tty_kb_buf = input;
      tty_kb_flag = 1;
      if( tty_dcr & TTY_IE_MASK ){
        cpu_raise_interrupt(TTYI_INTR_FLAG);
      }
    }
  }

  // If output teleprinter buffer if requested.
  if( output_pending ){
    output_pending = 0;
    write_tty_byte(tty_tp_buf);
    tty_tp_flag = 1;
    if( tty_dcr & TTY_IE_MASK ){
      cpu_raise_interrupt(TTYO_INTR_FLAG);
    }
  }
}
