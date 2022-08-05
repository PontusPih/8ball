/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#include "tty.h"
#include "cpu.h"

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

void tty_reset()
{
  tty_kb_buf = 0;
  tty_kb_flag = 0;
  tty_tp_buf = 0;
  tty_tp_flag = 0;
  tty_dcr = TTY_IE_MASK;
}

void tty_kb_process(char input)
{
  // TTY keyboard handling

  tty_kb_buf = input;
  tty_kb_flag = 1;
  if( tty_dcr & TTY_IE_MASK ){
    cpu_raise_interrupt(TTYI_INTR_FLAG);
  }
}

char tty_tp_process(char *output)
{
  // TTY teleprinter handling
  if( output_pending ){
    *output = tty_tp_buf;
    output_pending = 0;
    tty_tp_flag = 1;
    if( tty_dcr & TTY_IE_MASK ){
      cpu_raise_interrupt(TTYO_INTR_FLAG);
    }
    return 1; // Request output of char
  }
  
  return 0;
}
