/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#ifndef _SERIAL_COM_H_
#define _SERIAL_COM_H_

void serial_setup(char* linename);
void serial_teardown();
void send_cmd(unsigned char *cmd, int len);
int recv_cmd(unsigned char *out_buf);
void send_console_break();
char recv_console_break();

#endif // _SERIAL_COM_H_
