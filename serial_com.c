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

// These functions are platform specific to communcation mechanism and
// must be implemented elsewhere.
void write_byte(char byte);
char read_byte();
char find_byte_nonblocking(char target);
void channel_setup(char* linename);
void channel_teardown();

#define START_FRAME ('{')
#define END_FRAME   ('}')
#define ESCAPE      ('~')
#define CONSOLE     ('.')

void serial_setup(char* linename)
{
  channel_setup(linename);
}

void serial_teardown()
{
  channel_teardown();
}

char serial_recv_break()
{
  return find_byte_nonblocking(CONSOLE);
}

// Send one frame, escape special characters.
void serial_send(unsigned char *cmd, int len)
{
  write_byte(START_FRAME);
  
  for( int i = 0; i < len; i++ ){
    switch(cmd[i]){
    case START_FRAME:
    case END_FRAME:
    case ESCAPE:
    case CONSOLE:
      write_byte(ESCAPE);
      break;
    }
    write_byte(cmd[i]);
  }

  write_byte(END_FRAME);
}


// Send console break character.
void serial_send_break()
{
  write_byte(CONSOLE);
}


// Recieve one frame. The somewhat convoluted code waits for a
// "START_FRAME" byte and will remove ESCAPE-bytes in the content.

// TODO consider prohibiting special chars inside frames. Then a lost
//      escape char is less problematic.

// If any error is detected such as an unexpected special character or
// byte might be lost we reset the state and wait for a new frame. It
// is up to the sender to retry or go to console mode.
int serial_recv(unsigned char *out_buf)
{
  int i = 0;
  unsigned char byte;
  enum { WAIT, FRAME, ESCAPED } state = WAIT;
  do{
    byte = read_byte();
    
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
        return i; // Frame successfully read. Set output pointer,
                  // return content length.
      }
      break;
    }

    if( state == ESCAPED ){
      byte = read_byte();
      switch(byte) {
      case START_FRAME:
      case END_FRAME:
      case ESCAPE:
      case CONSOLE:
        state = FRAME;
        break; // ESCAPE dropped continue
      default:
        // Only special bytes are escaped, a byte might be lost,
        // wait for a new frame.
        state = WAIT;
        break;
      }
    }

    if( state == FRAME ) {
      out_buf[i++] = byte;
    }
    
  } while(1);

  // Oops, this is a bug.
  return -1;
}
