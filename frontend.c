/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#include "frontend.h"

#ifdef PTY_CLI
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "serial_com.h"
#else
#include "backend.h"
#endif

#define UNUSED(x) (void)(x);

void frontend_setup(char *backend_address)
{
#ifdef PTY_CLI
  serial_setup(backend_address);

  for( int i = 0; i < 5; i++ ){
    serial_send_break();
    sleep(1);
    if( serial_recv_break() ){
      return;
    }
  }

  printf("Unable to do handshake with backend\n");
  exit(EXIT_FAILURE);
#else
  UNUSED(backend_address); // To avoid warning.
  backend_setup();
#endif
}


void frontend_cleanup()
{
#ifdef PTY_CLI
  serial_teardown();
#endif
}


short frontend_send_receive(unsigned char *sbuf, int slen, unsigned char *rbuf)
{
#ifdef PTY_CLI
  static char NO_ENTER = 0;
  if( NO_ENTER == 1 ){
    printf("GAH! Don't call frontend_send_receive twice!\n");
    exit(EXIT_FAILURE);
  }
  NO_ENTER = 1;

  serial_send(sbuf,slen);
  short reply_length = serial_recv(rbuf);

  NO_ENTER = 0;
  return reply_length;
#else
  UNUSED(slen);
  int rlen;
  backend_dispatch(sbuf, rbuf, &rlen);
  return rlen;
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
