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
int pts = -1; // PTY slave handle

#include "machine.h"
#include "serial_com.h"

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
}


char frontend_run(char single)
{
  unsigned char run_buf[1] = { single ? 'S' : 'R' };
  send_cmd(pts, run_buf, 1);

  while(1) {
    unsigned char *buf;
    recv_cmd(pts, &buf);
    switch(buf[0]) {
    case 'I': // Interrupted
      send_cmd(pts, (unsigned char*)"C",1);
      __attribute__ ((fallthrough));
    case 'H': // CPU has halted
    case 'B': // Breakpoint hit
    case 'S': // Single step done
    case 'P': // stop_at hit
      return buf[0];
      break;
    case 'T': // TTY Request
      if( buf[1] == 'R' ){
        char output;
        char res = console_read_tty_byte(&output);
        unsigned char buf[2] = { res, output };
        send_cmd(pts, buf, 2);
      } else {
        console_write_tty_byte(buf[2]);
      }
      break;
    case 'D': // Display (trace) instruction.
      // TODO BUG, console interrupt here is not acked.
      console_trace_instruction();
      // TODO handle this in console.c
      // During trace instruction the server pops out to console mode,
      // restart it.
      send_cmd(pts, run_buf, 1);
      break;
    }
  }
}


short buf2short(unsigned char *b, int i)
{
  return (b[i] << 8) | (b[i+1] & 0xFF);
}


void send_command(char a, char b)
{
  unsigned char buf[2] = { a, b };
  send_cmd(pts, buf, 2);
}


void send_char(char a, char b, char x)
{
  unsigned char buf[3] = { a, b, x };
  send_cmd(pts,buf,3);
}


void send_short(char a, char b, short x)
{
  unsigned char buf[4] = { a, b, x >> 8, x & 0xFF };
  send_cmd(pts, buf, 4);
}


void send_short_short(char a, char b, short x, short y)
{
  unsigned char buf[6] = { a, b, x >> 8, x & 0xFF, y >> 8, y & 0xFF };
  send_cmd(pts, buf, 6);
}


void send_char_short(char a, char b, char x, short y)
{
  unsigned char buf[5] = { a, b, x, y >> 8, y & 0xFF };
  send_cmd(pts,buf,5);
}


void send_short_char(char a, char b, short x, char y)
{
  unsigned char buf[5] = { a, b, x >> 8, x & 0xFF, y };
  send_cmd(pts,buf,5);
}


short receive_short()
{
  unsigned char *rbuf;
  recv_cmd(pts, &rbuf);
  return buf2short(rbuf, 0);
}


short frontend_examine_mem(short addr)
{
  send_short('E', 'M', addr);
  return receive_short();
}


void frontend_deposit_mem(short addr, short val)
{
  //printf("Send: %o, %o\n", addr, val);
  send_short_short('D','M', addr, val);
}


short frontend_operand_addr(short addr, char examine)
{
  send_short_char('E', 'O', addr, examine);
  return receive_short();
}


short frontend_direct_addr(short addr)
{
  send_short('E', 'D', addr);
  return receive_short();
}


void frontend_deposit_reg(register_name_t reg, short val)
{
  send_char_short('D', 'R', reg, val);
}


short frontend_examine_reg(register_name_t reg)
{
  send_char('E', 'R', reg);
  return receive_short();
}


short frontend_examine_bp(short addr)
{
  send_short('E', 'B', addr);
  return receive_short();
}


void frontend_toggle_bp(short addr)
{
  send_short('D', 'B', addr);
}


short frontend_examine_trace()
{
  send_command('E', 'T');
  return receive_short();
}


void frontend_toggle_trace()
{
  send_command('D', 'T');
}


void frontend_set_stop_at(short addr)
{
  send_short('D', 'P', addr);
}


void frontend_quit()
{
  unsigned char buf[1] = { 'Q' };
  send_cmd(pts, buf, 1);
}


void frontend_interrupt()
{
  send_console_break(pts);
  send_console_break(pts);
  // TODO, keep sending breaks?
  while(1) {
    // system interrupted. await acknowledge
    unsigned char *rbuf;
    recv_cmd(pts, &rbuf);
    if(rbuf[0] == 'I'){
      unsigned char buf[1] =  { 'C' };
      send_cmd(pts, buf, 1);
      break;
    }
  }
}

#endif
