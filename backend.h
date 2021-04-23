/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#ifndef _BACKEND_H_
#define _BACKEND_H_

#include "machine.h"

void backend_setup();
void backend_main();
short backend_examine_deposit_reg(register_name_t reg, short val, char dep);
char backend_run(char single);
void backend_clear_all_bp();
short backend_examine_bp(short addr);
void backend_toggle_bp(short addr);
void backend_set_stop_at(short addr);
void backend_interrupt();
char backend_read_tty_byte(char *output);
char backend_write_tty_byte(char output);

void backend_dispatch(unsigned char *buf, unsigned char *reply_buf, int *reply_length);

#endif // _BACKEND_H_
