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

#endif

#include "machine.h"
#include "serial_com.h"
#include "frontend.h"
#include "backend.h"

char in_console_mode = 1;
char interrupt_console_mode = 0;

#ifdef PTY_CLI
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
#endif

static short buf2short(unsigned char *b, int i)
{
  return (b[i] << 8) | (b[i+1] & 0xFF);
}


void frontend_send(__attribute__((unused))unsigned char *buf, __attribute__((unused))int len)
{
#ifdef PTY_CLI
  static char NO_ENTER = 0;
  if( NO_ENTER == 1 ){
    printf("GAH! Don't call front_end_send_receive twice!\n");
    exit(EXIT_FAILURE);
  }

  NO_ENTER = 1;
  send_cmd(pts,buf,len);
  NO_ENTER = 0;
#endif
}

short frontend_dispatch_internal(unsigned char *reply_buf);

short frontend_receive(__attribute__((unused))unsigned char *buf,__attribute__((unused)) int len)
{
#ifdef PTY_CLI
  static char NO_ENTER = 0;

  if( NO_ENTER == 1 ){
    printf("GAH! Don't call frontend_receive twice!\n");
    exit(EXIT_FAILURE);
  }

  NO_ENTER = 1;

  unsigned char rbuf[128];
  if( recv_cmd(pts, rbuf) == -1 ) {
    // backend acknowledged console mode.
    printf("Backend wants console mode\n");
    NO_ENTER = 0;
    return 'I';
  }
  // printf("Got Reply %c\n",rbuf[0]);

  if( in_console_mode ){
    if( rbuf[0] != 'V' ){
      // Frontend is in console mode but backend is not
      // Try to reconnect and resend
      frontend_connect_or_die();
      frontend_send(buf, len);
      recv_cmd(pts, rbuf);
      if( rbuf[0] != 'V' ){
	printf("Communcations error\n");
	exit(EXIT_FAILURE);
      }
    }
  }

  short res = frontend_dispatch_internal(rbuf);
  NO_ENTER = 0;
  return res;
#else
  return 0;
#endif
}


short frontend_dispatch_internal(unsigned char *reply_buf)
{
  switch(reply_buf[0]) {
  case 'V': // Value, for console examine
    return buf2short(reply_buf, 1);
    break;
  case 'I': // Interrupted
  case 'H': // CPU has halted
  case 'B': // Breakpoint hit
  case 'S': // Single step done
  case 'P': // stop_at hit
    in_console_mode = 1;
    return reply_buf[0];
    break;
  case 'T': // TTY Request
    if( reply_buf[1] == 'R' ){
      // char output;
      // char res = machine_read_tty_byte(&output);
      // unsigned char rbuf[3] = { 'V', res, output };*/
    } else {
      // console_write_tty_byte(rbuf[2]);
    }
    break;
  }
  return 0;
}


char frontend_run(char single)
{
  unsigned char run_buf[1] = { single ? 'S' : 'R' };
  short res;
  in_console_mode = 0;

  while( 1 ) {
#ifdef PTY_CLI
    frontend_send(run_buf, 1);
    res = 1 ; //frontend_dispatch(run_buf);
#else
    unsigned char rbuf[3];
    int rlen;
    backend_dispatch(run_buf, rbuf, &rlen);
    res = frontend_dispatch_internal(rbuf);
#endif
    if( in_console_mode ){
      return res;
    }
    if( interrupt_console_mode ){
      interrupt_console_mode = 0;
      in_console_mode = 1;
#ifdef PTY_CLI      
      frontend_connect_or_die();
#endif
      return 'I';
    }
  }
}

#ifdef PTY_CLI      
void frontend_dispatch(unsigned char *send_buf, int send_length, unsigned char *reply_buf, char expect_reply)
{
  send_cmd(pts, send_buf, send_length);
  if( expect_reply ){
    if( recv_cmd(pts, reply_buf) < 0){
      reply_buf[0] = 'I';
    }
  }
}
#endif


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
  send_console_break(pts);
  recv_console_break(pts);
}

