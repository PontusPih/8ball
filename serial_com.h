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
void serial_send(unsigned char *cmd, int len);
int serial_recv(unsigned char *out_buf);
void serial_send_break();
char serial_recv_break();

#endif // _SERIAL_COM_H_
