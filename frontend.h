/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#ifndef _FRONTEND_H_
#define _FRONTEND_H_

void frontend_setup(char *backend_address);
void frontend_cleanup();
void frontend_interrupt();
short frontend_send_receive(unsigned char *buf, int len, unsigned char *rbuf);

#endif // _FRONTEND_H_
