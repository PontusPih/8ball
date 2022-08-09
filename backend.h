/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#ifndef _BACKEND_H_
#define _BACKEND_H_

char backend_setup();
void backend_clear_all_bp();
void backend_interrupt();
// TODO remove
char backend_read_tty_byte(char *output);
char backend_write_tty_byte(char output);

void backend_dispatch(unsigned char *send_buf, int send_length, unsigned char *reply_buf, int *reply_length);

#endif // _BACKEND_H_
