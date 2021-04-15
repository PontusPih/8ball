/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

// This files implements a simple framing protocol for serial
// communications.
//
// Every array of bytes(the content) sent is put in a frame:
// '{' + content + '}'
//
// The special character '.' is intended to put the server side in
// "console" mode.
//
// Special characters in the contents are escaped with '~'
//
// The file descriptor used for communications is assumed to be in
// non-blocking mode.

#include "serial_com.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#define START_FRAME ('{')
#define END_FRAME   ('}')
#define ESCAPE      ('~')
#define CONSOLE     ('.')

// #define DEBUG_PRINT

// "blocking" write of single byte
static void write_byte(int fd, char byte)
{
  ssize_t rlen = write(fd, &byte, 1);

  if( rlen < 1 ){
    printf("Unable to write to PTY\n");
    if( rlen < 0 ){
      perror(__func__);
    }
    exit(EXIT_FAILURE);
  }
}

// Send one frame, escape special characters.
void send_cmd(int fd, unsigned char *cmd, int len)
{
  write_byte(fd, START_FRAME);
#ifdef DEBUG_PRINT
  printf("Sent: %c ", START_FRAME);
#endif
  
  for( int i = 0; i < len; i++ ){
    switch(cmd[i]){
    case START_FRAME:
    case END_FRAME:
    case ESCAPE:
    case CONSOLE:
#ifdef DEBUG_PRINT
      printf("%c ", ESCAPE);
#endif
      write_byte(fd, ESCAPE);
      break;
    }
#ifdef DEBUG_PRINT
    if( isprint(cmd[i]) ){
      printf("%c ", cmd[i]);
    } else {
      printf("%x ", cmd[i]);
    }
#endif
    write_byte(fd, cmd[i]);
  }

  write_byte(fd, END_FRAME);
#ifdef DEBUG_PRINT
  printf("%c\n", END_FRAME);
#endif
}


// Send console break character.
void send_console_break(int fd)
{
  write_byte(fd, CONSOLE);
  printf(" BREAK: %s \n", PTY_CLI);
}


// Check for console character. read must be nonblocking.
char recv_console_break(int fd)
{
  char byte;
  ssize_t len = 0;
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  do {
    len = read(fd, &byte, 1);
  } while ( len == 1 && byte != CONSOLE );
  fcntl(fd, F_SETFL, flags);

  return len == 1 && byte == CONSOLE;
}


// "blocking" read of one byte
static char read_byte(int fd)
{
  char byte;
  ssize_t len = 0;
  do {
    len = read(fd, &byte, 1);
  } while( len == 0 );

  if( len < 0 ){
    printf("Unable to read from PTY\n");
    perror(__func__);
    exit(EXIT_FAILURE);
  }
  return byte;
}


// Recieve one frame. The somewhat convoluted code waits for a
// "START_FRAME" byte and will remove ESCAPE-bytes in the content.

// If any error is detected such as an unexpected special character or
// byte might be lost we reset the state and wait for a new frame. It
// is up to the sender to retry or go to console mode.
int recv_cmd(int fd, unsigned char **out_buf)
{
  static unsigned char buf[128];

  int i = 0;
  unsigned char byte;
  enum { WAIT, FRAME, ESCAPED } state = WAIT;
#ifdef DEBUG_PRINT
  printf("Recv: ");
#endif 
  do{
    byte = read_byte(fd);
#ifdef DEBUG_PRINT
    if( isprint(byte) ){
      printf("%c ", byte);
    } else {
      printf("%x ", byte);
    }
#endif
    
    switch (byte) {
    case START_FRAME:
      i = 0;
      state = FRAME;
      continue;   // Frame start or restart, read next byte.
    case ESCAPE:
      if( state == FRAME ) {
        state = ESCAPED;
      }
      break;      // Handle escaped byte below
    case CONSOLE:
      return -1;  // Drop to console. No data returned
    case END_FRAME:
      if( FRAME ){
        *out_buf = buf;
#ifdef DEBUG_PRINT
	printf("\n");
#endif
        return i; // Frame successfully read. Set output pointer,
                  // return content length.
      }
      break;
    }

    if( state == ESCAPED ){
      byte = read_byte(fd);
      switch(byte) {
      case START_FRAME:
      case END_FRAME:
      case ESCAPE:
      case CONSOLE:
        state = FRAME;
#ifdef DEBUG_PRINT
        if( isprint(byte) ){
          printf("%c ", byte);
        } else {
          printf("%x ", byte);
        }
#endif
        break; // ESCAPE dropped continue
      default:
        // Only special bytes are escaped, a byte might be lost,
        // wait for a new frame.
        state = WAIT;
        break;
      }
    }

    if( state == FRAME ) {
      buf[i++] = byte;
    }
    
  } while(1);

  // Oops, this is a bug.
  exit(EXIT_FAILURE);
}
