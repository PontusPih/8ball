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
#include "frontend.h"

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

  frontend_interrupt(1); // Ensure we are in console mode
}


char frontend_run(char single)
{
  unsigned char run_buf[1] = { single ? 'S' : 'R' };
  send_cmd(pts, run_buf, 1);

  while(1) {
    unsigned char *buf;
    if( recv_cmd(pts, &buf) == -1 ) {
      // backend acknowledged console mode.
      printf("Backend wants console mode\n");
      return 'I';
    }
    switch(buf[0]) {
    case 'I': // Interrupted
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
        unsigned char buf[3] = { 'V', res, output };
        send_cmd(pts, buf, 3);
      } else {
        console_write_tty_byte(buf[2]);
      }
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
  if( recv_cmd(pts, &rbuf) == -1 ){
    printf("OOPS! Backend wants console mode\n");
    return 07777;
  }
  switch(rbuf[0]) {
  case 'V': // Received short value as expected
    return buf2short(rbuf, 1);
    break;
  default:
    // Any other result means the backend is not in console mode.
    printf("OOPS! Backend not in console mode! got %c%c%c\n", rbuf[0], rbuf[1], rbuf[2]);
    frontend_interrupt(1);
    break;
  }
  return 07777;
}


short frontend_examine_mem(short addr)
{
  send_short('E', 'M', addr);
  return receive_short();
}


void frontend_deposit_mem(short addr, short val)
{
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


void frontend_set_stop_at(short addr)
{
  send_short('D', 'P', addr);
}


void frontend_quit()
{
  unsigned char buf[1] = { 'Q' };
  send_cmd(pts, buf, 1);
}


void frontend_interrupt(char wait)
{
  unsigned char *rbuf;

  send_console_break(pts);
  if( wait ){
    while( recv_cmd(pts, &rbuf) != -1 ){}
  }
}

#endif
