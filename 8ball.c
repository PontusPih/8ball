#include "cpu.h"
#include "tty.h"
#include "console.h"

int main (int argc, char **argv)
{
#include "rimloader.h"
  pc = 07756;

  console_setup(argc, argv);
  tty_reset();

  int tty_skip_count = 0;
  while(1){

    if( tty_skip_count++ >= 1 ){ // TODO simulate slow TTY (update maindec-d0cc to do all loops)
      tty_skip_count = 0;
      tty_process();
    }

    console_stop_break();

    if( in_console ){
      in_console = console();
    }

    console_trace_instruction();
    
    if( cpu_process() == -1 ){
      in_console = -1;
    }
  }
}



