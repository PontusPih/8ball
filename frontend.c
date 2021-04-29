/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#ifdef PTY_CLI

#include "console.h"
#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#define _BSD_SOURCE 1
#define __USE_MISC 1
#include <termios.h>
#include <unistd.h>
int pts = -1; // PTY slave handle

#include "machine.h"
#include "serial_com.h"
#include "frontend.h"

void frontend_connect_or_die()
{
  for( int i = 0; i < 5; i++ ){
    send_console_break(pts);
    sleep(1);
    if( recv_console_break(pts) ){
      return;
    }
  }

  printf("Unable to do handshake with backend\n");
  exit(EXIT_FAILURE);
}


void frontend_setup(char *pty_name)
{
  if( pty_name == NULL ){
    printf("Please give a PTY name\n");
    exit(EXIT_FAILURE);
  }
  pts = open(pty_name, O_RDWR|O_NOCTTY);
  
  if( pts == -1 ){
    printf("Unable to open %s\n", pty_name);
    exit(EXIT_FAILURE);
  }
  
  struct termios cons_old_settings;
  struct termios cons_new_settings;
  tcgetattr(pts, &cons_old_settings);
  cons_new_settings = cons_old_settings;
  cfmakeraw(&cons_new_settings);
  tcsetattr(pts, TCSANOW, &cons_new_settings);

  frontend_connect_or_die();
}


void frontend_cleanup()
{
  close(pts);
}


short frontend_send_receive(unsigned char *sbuf, int slen, unsigned char *rbuf)
{
  static char NO_ENTER = 0;
  if( NO_ENTER == 1 ){
    printf("GAH! Don't call frontend_send_receive twice!\n");
    exit(EXIT_FAILURE);
  }
  NO_ENTER = 1;

  send_cmd(pts,sbuf,slen);
  short recv_res = recv_cmd(pts, rbuf);

  NO_ENTER = 0;
  return recv_res;
}


void frontend_interrupt()
{
  send_console_break(pts);
}

#endif
