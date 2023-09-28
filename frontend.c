/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#include "frontend.h"

#ifdef PTY_CLI
#include "serial_com.h"
#else
#include "backend.h"
#endif

#include <unistd.h>

#define UNUSED(x) (void)(x);

char frontend_setup(char *backend_address)
{
#ifdef PTY_CLI
  serial_setup(backend_address);

  for( int i = 0; i < 5; i++ ){
    serial_send_break();
    sleep(1);
    if( serial_recv_break() ){
      return 1;
    }
  }

  return -1;
#else
  UNUSED(backend_address); // To avoid warning.
  return backend_setup();
#endif
}


void frontend_cleanup()
{
#ifdef PTY_CLI
  serial_teardown();
#endif
}


void frontend_send_receive(unsigned char *send_buf, int send_length, unsigned char *reply_buf, int *reply_length)
{
#ifdef PTY_CLI
  static char NO_ENTER = 0;
  if( NO_ENTER == 1 ){
    reply_buf[0] = 'E';
    *reply_length = 1;
    return;
  }
  NO_ENTER = 1;

  serial_send(send_buf, send_length);
  *reply_length = serial_recv(reply_buf);

  NO_ENTER = 0;
#else
  backend_dispatch(send_buf, send_length, reply_buf, reply_length);
#endif
}


void frontend_interrupt()
{
#ifdef PTY_CLI
  serial_send_break();
#else
  backend_interrupt();
#endif
}
