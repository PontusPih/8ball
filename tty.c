/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#include "tty.h"
#include "cpu.h"
#include "backend.h"

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
  // TTY and console handling
  char res = 0;

  // If keyboard flag is not set, try to read one char.
  if( !tty_kb_flag ) {
    char input;
    if( backend_read_tty_byte(&input) ) {
      // A char has been received
      tty_kb_buf = input;
      tty_kb_flag = 1;
      if( tty_dcr & TTY_IE_MASK ){
        cpu_raise_interrupt(TTYI_INTR_FLAG);
      }
    } else {
      res = 1; // Request service
    }
  }
  
  // If output teleprinter buffer if requested.
  if( output_pending ){
    if( backend_write_tty_byte(tty_tp_buf) ){
      // tty_tp_buf or earlier char has been written
      output_pending = 0;
      tty_tp_flag = 1;
      if( tty_dcr & TTY_IE_MASK ){
	cpu_raise_interrupt(TTYO_INTR_FLAG);
      }
    } else {
      res = 1; // Request service
    }
  }
  
  return res;
}
