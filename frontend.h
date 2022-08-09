/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#ifndef _FRONTEND_H_
#define _FRONTEND_H_

char frontend_setup(char *backend_address);
void frontend_cleanup();
void frontend_interrupt();
void frontend_send_receive(unsigned char *send_buf, int send_len, unsigned char *reply_buf, int *reply_length);

#endif // _FRONTEND_H_
