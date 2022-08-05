/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#ifdef PTY_CLI
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "console.h"
#include "machine.h"
#include "serial_com.h"
#include "frontend.h"

void frontend_setup(char *backend_address)
{
  serial_setup(backend_address);

  for( int i = 0; i < 5; i++ ){
    send_console_break();
    sleep(1);
    if( recv_console_break() ){
      return;
    }
  }

  printf("Unable to do handshake with backend\n");
  exit(EXIT_FAILURE);
}


void frontend_cleanup()
{
  serial_teardown();
}


short frontend_send_receive(unsigned char *sbuf, int slen, unsigned char *rbuf)
{
  static char NO_ENTER = 0;
  if( NO_ENTER == 1 ){
    printf("GAH! Don't call frontend_send_receive twice!\n");
    exit(EXIT_FAILURE);
  }
  NO_ENTER = 1;

  send_cmd(sbuf,slen);
  short recv_res = recv_cmd(rbuf);

  NO_ENTER = 0;
  return recv_res;
}


void frontend_interrupt()
{
  send_console_break();
}

#endif
