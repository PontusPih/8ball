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


void frontend_disconnect()
{
  send_console_break(pts);
  if( recv_console_break(pts) ){
    return;
  }
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


static short buf2short(unsigned char *b, int i)
{
  return (b[i] << 8) | (b[i+1] & 0xFF);
}


short frontend_send_receive(unsigned char *buf, int len)
{
  static char NO_ENTER = 0;
  int result = 0;

  if( NO_ENTER == 1 ){
    printf("GAH! Don't call frontend_send_receive twice!\n");
    exit(EXIT_FAILURE);
  }
  NO_ENTER = 1;

  send_cmd(pts,buf,len);

  unsigned char rbuf[128];
  rbuf[0] = 'X';
  int recv_res = recv_cmd(pts, rbuf);

  switch( rbuf[0] ){
  case 'V': // Value, for console examine
    result = buf2short(rbuf, 1);
    break;
  case 'A': // Acknowledge, for console deposit
    break;
  default:
    printf("BAD STATE, backend sent:");
    if( recv_res < 0 ){
      printf(" console break\n");
    } else {
      printf(" '%c'\n", rbuf[0]);
    }
    exit(EXIT_FAILURE);
    break;
  }

  NO_ENTER = 0;
  return result;
}


void frontend_dispatch(unsigned char *send_buf, int send_length, unsigned char *reply_buf, char expect_reply)
{
  send_cmd(pts, send_buf, send_length);
  if( expect_reply ){
    if( recv_cmd(pts, reply_buf) < 0){
      reply_buf[0] = 'I';
    }
  }
}


short frontend_examine_mem(short addr)
{
  unsigned char buf[4] = { 'E', 'M', addr >> 8, addr & 0xFF };
  return frontend_send_receive(buf, sizeof(buf));
}


void frontend_deposit_mem(short addr, short val)
{
  unsigned char buf[6] = { 'D', 'M', addr >> 8, addr & 0xFF, val >> 8, val & 0xFF };
  frontend_send_receive(buf, sizeof(buf));
}


short frontend_operand_addr(short addr, char examine)
{
  unsigned char buf[5] = { 'E', 'O', addr >> 8, addr & 0xFF, examine };
  return frontend_send_receive(buf, sizeof(buf));
}


short frontend_direct_addr(short addr)
{
  unsigned char buf[4] = { 'E', 'D', addr >> 8, addr & 0xFF };
  return frontend_send_receive(buf, sizeof(buf));
}


void frontend_deposit_reg(register_name_t reg, short val)
{
  unsigned char buf[5] = { 'D', 'R', reg, val >> 8, val & 0xFF };
  frontend_send_receive(buf, sizeof(buf));
}


short frontend_examine_reg(register_name_t reg)
{
  unsigned char buf[3] = {'E', 'R', reg};
  return frontend_send_receive(buf, sizeof(buf));
}


short frontend_examine_bp(short addr)
{
  unsigned char buf[4] = { 'E', 'B', addr >> 8, addr & 0xFF };
  return frontend_send_receive(buf, sizeof(buf));
}


void frontend_toggle_bp(short addr)
{
  unsigned char buf[4] = { 'D', 'B', addr >> 8, addr & 0xFF };
  frontend_send_receive(buf, sizeof(buf));
}


void frontend_set_stop_at(short addr)
{
  unsigned char buf[4] = { 'D', 'P', addr >> 8, addr & 0xFF };
  frontend_send_receive(buf, sizeof(buf));
}


void frontend_interrupt()
{
  send_console_break(pts);
  recv_console_break(pts);
}

#endif
