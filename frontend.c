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

char in_console_mode = 1;
char interrupt_console_mode = 0;

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


short buf2short(unsigned char *b, int i)
{
  return (b[i] << 8) | (b[i+1] & 0xFF);
}


void frontend_send(unsigned char *buf, int len)
{
  static char NO_ENTER = 0;
  if( NO_ENTER == 1 ){
    printf("GAH! Don't call front_end_send_receive twice!\n");
    exit(EXIT_FAILURE);
  }

  NO_ENTER = 1;
  send_cmd(pts,buf,len);
  NO_ENTER = 0;
}

short frontend_receive(unsigned char *buf, int len)
{
  static char NO_ENTER = 0;

  if( NO_ENTER == 1 ){
    printf("GAH! Don't call frontend_receive twice!\n");
    exit(EXIT_FAILURE);
  }

  NO_ENTER = 1;

  unsigned char *rbuf;
  if( recv_cmd(pts, &rbuf) == -1 ) {
    // backend acknowledged console mode.
    printf("Backend wants console mode\n");
    NO_ENTER = 0;
    return 'I';
  }
  printf("Got Reply %c\n",rbuf[0]);

  if( in_console_mode ){
    if( rbuf[0] != 'V' ){
      // Frontend is in console mode but backend is not
      // Try to reconnect and resend
      frontend_connect_or_die();
      frontend_send(buf, len);
      recv_cmd(pts, &rbuf);
      if( rbuf[0] != 'V' ){
	printf("Communcations error\n");
	exit(EXIT_FAILURE);
      }
    }
    NO_ENTER = 0;
    return buf2short(rbuf, 1);
  } else {
    switch(rbuf[0]) {
    case 'I': // Interrupted
    case 'H': // CPU has halted
    case 'B': // Breakpoint hit
    case 'S': // Single step done
    case 'P': // stop_at hit
      NO_ENTER = 0;
      in_console_mode = 1;
      return rbuf[0];
      break;
    case 'T': // TTY Request
      if( rbuf[1] == 'R' ){
	// char output;
	// char res = console_read_tty_byte(&output);
	// unsigned char rbuf[3] = { 'V', res, output };
      } else {
	// console_write_tty_byte(rbuf[2]);
      }
      break;
    }
  }

  NO_ENTER = 0;
  return 0;
}


char frontend_run(char single)
{
  unsigned char run_buf[1] = { single ? 'S' : 'R' };
  short res;
  in_console_mode = 0;

  while( 1 ) {
    frontend_send(run_buf, 1);
    res = frontend_receive(run_buf,1);
    if( in_console_mode ){
      return res;
    }
    if( interrupt_console_mode ){
      interrupt_console_mode = 0;
      in_console_mode = 1;
      frontend_connect_or_die();
      return 'I';
    }
  }
}


short frontend_examine_mem(short addr)
{
  unsigned char buf[4] = { 'E', 'M', addr >> 8, addr & 0xFF };
  frontend_send(buf, sizeof(buf));

  return frontend_receive(buf, sizeof(buf));
}


void frontend_deposit_mem(short addr, short val)
{
  unsigned char buf[6] = { 'D', 'M', addr >> 8, addr & 0xFF, val >> 8, val & 0xFF };
  frontend_send(buf, sizeof(buf));
}


short frontend_operand_addr(short addr, char examine)
{
  unsigned char buf[5] = { 'E', 'O', addr >> 8, addr & 0xFF, examine };
  frontend_send(buf, sizeof(buf));

  return frontend_receive(buf, sizeof(buf));
}


short frontend_direct_addr(short addr)
{
  unsigned char buf[4] = { 'E', 'D', addr >> 8, addr & 0xFF };
  frontend_send(buf, sizeof(buf));

  return frontend_receive(buf, sizeof(buf));
}


void frontend_deposit_reg(register_name_t reg, short val)
{
  unsigned char buf[5] = { 'D', 'R', reg, val >> 8, val & 0xFF };
  frontend_send(buf, sizeof(buf));
}


short frontend_examine_reg(register_name_t reg)
{
  unsigned char buf[3] = {'E', 'R', reg};
  frontend_send(buf, sizeof(buf));

  return frontend_receive(buf, sizeof(buf));
}


short frontend_examine_bp(short addr)
{
  unsigned char buf[4] = { 'E', 'B', addr >> 8, addr & 0xFF };
  frontend_send(buf, sizeof(buf));
  return frontend_receive(buf, sizeof(buf));
}


void frontend_toggle_bp(short addr)
{
  unsigned char buf[4] = { 'D', 'B', addr >> 8, addr & 0xFF };
  frontend_send(buf, sizeof(buf));
}


void frontend_set_stop_at(short addr)
{
  unsigned char buf[4] = { 'D', 'P', addr >> 8, addr & 0xFF };
  frontend_send(buf, sizeof(buf));
}


void frontend_interrupt()
{
  interrupt_console_mode = 1;
}
#endif
