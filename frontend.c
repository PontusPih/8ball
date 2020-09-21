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

int in_console_mode = 1;

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

short frontend_receive()
{
  static char NO_ENTER = 0;

  if( NO_ENTER == 1 ){
    printf("GAH! Don't call front_end_send_receive twice!\n");
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
  switch(rbuf[0]) {
  case 'I': // Interrupted
  case 'H': // CPU has halted
  case 'B': // Breakpoint hit
  case 'S': // Single step done
  case 'P': // stop_at hit
    NO_ENTER = 0;
    return rbuf[0];
    break;
  case 'V':
    NO_ENTER = 0;
    return buf2short(rbuf, 1);
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
  NO_ENTER = 0;
  return 0;
}


char frontend_run(char single)
{
  unsigned char run_buf[1] = { single ? 'S' : 'R' };
  in_console_mode = 0;

  while( !in_console_mode ) {
    frontend_send(run_buf, 1);
    frontend_receive();
  }

  // TODO send break ?
  return 0;
}


void send_char(char a, char b, char x)
{
  unsigned char buf[3] = { a, b, x };
  frontend_send(buf,3);
}


void send_short(char a, char b, short x)
{
  unsigned char buf[4] = { a, b, x >> 8, x & 0xFF };
  frontend_send(buf, 4);
}


void send_short_short(char a, char b, short x, short y)
{
  unsigned char buf[6] = { a, b, x >> 8, x & 0xFF, y >> 8, y & 0xFF };
  frontend_send(buf, 6);
}


void send_char_short(char a, char b, char x, short y)
{
  unsigned char buf[5] = { a, b, x, y >> 8, y & 0xFF };
  frontend_send(buf,5);
}


void send_short_char(char a, char b, short x, char y)
{
  unsigned char buf[5] = { a, b, x >> 8, x & 0xFF, y };
  frontend_send(buf,5);
}


/*
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
*/


short frontend_examine_mem(short addr)
{
  send_short('E', 'M', addr);
  return frontend_receive();
}


void frontend_deposit_mem(short addr, short val)
{
  send_short_short('D','M', addr, val);
}


short frontend_operand_addr(short addr, char examine)
{
  send_short_char('E', 'O', addr, examine);
  return frontend_receive();
}


short frontend_direct_addr(short addr)
{
  send_short('E', 'D', addr);
  return frontend_receive();
}


void frontend_deposit_reg(register_name_t reg, short val)
{
  send_char_short('D', 'R', reg, val);
}


short frontend_examine_reg(register_name_t reg)
{
  send_char('E', 'R', reg);
  return frontend_receive();
}


short frontend_examine_bp(short addr)
{
  send_short('E', 'B', addr);
  return frontend_receive();
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
  /*
    TODO
  unsigned char buf[1] = { 'Q', '0' };
  send_command('Q');
  return frontend_receive();*/
}


void frontend_interrupt(char wait)
{
  in_console_mode = 1;
  wait++;
  /*
  unsigned char *rbuf;

  send_console_break(pts);
  if( wait ){
    while( recv_cmd(pts, &rbuf) != -1 ){}
  }
  */
}
#endif
