#include "tty.h"
#include "cpu.h"
#include "machine.h"

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

char tty_process(){
  // TTY and console handling:
  // If keyboard flag is not set, try to read one char.
  if( !tty_kb_flag ) {
    char input, res;
    res = read_tty_byte(&input);
    if( res == 1 ) {
      tty_kb_buf = input;
      tty_kb_flag = 1;
    }
    if( res == -1 ){
      return -1;
    }
  }

  // If output teleprinter buffer if requested.
  if( output_pending ){
    output_pending = 0;
    write_tty_byte(tty_tp_buf);
    tty_tp_flag = 1;
  }

  return 0;
}
