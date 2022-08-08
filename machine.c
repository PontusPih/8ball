/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#include <stdio.h> // for RX and TTY file read
#include <stdlib.h> // For exit()
#include <unistd.h> // For read()

#include "cpu.h"
#include "tty.h"
#include "rx8.h"
#include "machine.h"

#include "frontend.h"

#define UNUSED(x) (void)(x);

void machine_setup(char *backend_address)
{
  frontend_setup(backend_address);
}


static short buf2short(unsigned char *b, int i)
{
  return (b[i] << 8) | (b[i+1] & 0xFF);
}


short machine_dispatch(unsigned char *sbuf, int slen, unsigned char *rbuf)
{
  return frontend_send_receive(sbuf, slen, rbuf);
}


short machine_interact(unsigned char *send_buf, int send_len)
{
  short result = 0;
  unsigned char rbuf[3];

  short reply_length = frontend_send_receive(send_buf, send_len, rbuf);

  if( reply_length < 0 ){
    rbuf[0] = 'X'; // Ensure buf does not contain 'V' or 'A'
  }
  
  switch( rbuf[0] ){
  case 'V': // Value for console examine
    result = buf2short(rbuf, 1);
    break;
  case 'A': // Acknowledge, for console deposit
    break;
  case 'E': // Something went wrong in backend
  default:
    printf("BAD STATE, backend sent:");
    if( rbuf[0] == 'X' ){
      printf(" console break\n");
    } else {
      printf(" '%c'\n", rbuf[0]);
    }
    exit(EXIT_FAILURE);
    break;
  }
  
  return result;
}

char machine_run(char single)
{
  // State change is RUN -> IO -> CONSOLE
  // or RUN -> IO -> RUN
  enum { RUN, IO, CONSOLE } state = RUN;
  unsigned char run_buf[1] = { single ? 'S' : 'R' };
  unsigned char io_buf[2];
  unsigned int io_len = 0;
  unsigned char reply_buf[128];
  reply_buf[0] = 'X';

  while( 1 ){
    if( state == IO ){
      if( machine_dispatch(io_buf, io_len, reply_buf) < 0 ){
	state = CONSOLE;
	continue;
      }
    } else {
      if( machine_dispatch(run_buf, 1, reply_buf) < 0 ){
	return 'I';
      }
    }

    if( run_buf[0] == 'R' ){
      run_buf[0] = 'C'; // After first R, issue continues
    }

    // There is always something in reply_buf here
    switch(reply_buf[0]){
    case 'T':
      switch(reply_buf[1]){
      case 'R':
	{
	  char output;
	  char res = machine_read_tty_byte(&output);
	  if( res == -1 ){
	    return 'I';
	  }
	  if( res > 0 ){
	    io_buf[0] = 'T';
	    io_buf[1] = output;
	    io_len = 2;
	    state = IO;
	  }
	}
	break;
      case 'W':
	machine_write_tty_byte(reply_buf[2]);
	break;
      default:
	return 'X';
	break;
      }
      break;
    case 'A':
      // Acknowledge I/O, if running, continue
      if( state == CONSOLE ){
	return 'I';
      } else {
	state = RUN;
      }
      break;
    case 'E':
      // An error occured in the backend
      printf("Backend is in a bad state, suggest restart\n");
      return reply_buf[0];
      break;
    default:
      return reply_buf[0];
      break;
    }
  }
}


short machine_examine_mem(short addr)
{
  unsigned char buf[4] = { 'E', 'M', addr >> 8, addr & 0xFF };
  return machine_interact(buf, sizeof(buf));
}


void machine_deposit_mem(short addr, short val)
{
  unsigned char buf[6] = { 'D', 'M', addr >> 8, addr & 0xFF, val >> 8, val & 0xFF };
  machine_interact(buf, sizeof(buf));
}


short machine_operand_addr(short addr, char examine)
{
  unsigned char buf[5] = { 'E', 'O', addr >> 8, addr & 0xFF, examine };
  return machine_interact(buf, sizeof(buf));
}


short machine_direct_addr(short addr)
{
  unsigned char buf[4] = { 'E', 'D', addr >> 8, addr & 0xFF };
  return machine_interact(buf, sizeof(buf));
}


short machine_examine_reg(register_name_t regname)
{
  unsigned char buf[3] = {'E', 'R', regname};
  return machine_interact(buf, sizeof(buf));
}


void machine_deposit_reg(register_name_t regname, short val)
{
  unsigned char buf[5] = { 'D', 'R', regname, val >> 8, val & 0xFF };
  machine_interact(buf, sizeof(buf));
}


void machine_clear_all_bp()
{
  unsigned char buf[2] = { 'D', 'C' };
  machine_interact(buf, sizeof(buf));
}


short machine_examine_bp(short addr)
{
  unsigned char buf[4] = { 'E', 'B', addr >> 8, addr & 0xFF };
  return machine_interact(buf, sizeof(buf));
}


void machine_toggle_bp(short addr)
{
  unsigned char buf[4] = { 'D', 'B', addr >> 8, addr & 0xFF };
  machine_interact(buf, sizeof(buf));
}


void machine_set_stop_at(short addr)
{
  unsigned char buf[4] = { 'D', 'P', addr >> 8, addr & 0xFF };
  machine_interact(buf, sizeof(buf));
}


void machine_interrupt()
{
  frontend_interrupt();
}


void machine_cleanup()
{
  frontend_cleanup();
}


void machine_mount_rx_image(short drive, char *filename)
{
#ifdef PTY_CLI
  UNUSED(drive);
  UNUSED(filename);
  // TODO implement serial transfer of RX data
#else
  FILE *image = fopen(filename, "r");
  char buf[77*26*128] = {0};
  int bytes_left = 77*26*128;

  if( NULL == image ){
    perror("Unable to open RX image");
    return;
  } else {
    char *buf_ptr = buf;
    while(bytes_left) {
      int bytes_read = fread(buf_ptr, 1, bytes_left, image);
      if( bytes_read == 0 ) {
	perror("Unable to read from RX image");
	fclose(image);
	return;
      }
      bytes_left -= bytes_read;
      buf_ptr += bytes_read;
    }
    rx01_fill(drive, buf);
  }
  fclose(image);
#endif
}


static char *tty_file = NULL;
static char tty_read_from_file = 0;
static FILE *tty_fh = NULL;

char machine_read_tty_byte(char *output)
{
  if( tty_read_from_file ){
    int byte = fgetc( tty_fh );
    if( byte == EOF ){
      printf("Reached end of TTY file, dropping to console. "
             "Further reads will be from keyboard\n");
      fclose( tty_fh );
      tty_read_from_file = 0;
      return -1;
    } else {
      *output = byte;
      return 1;
    }
  } else {
    unsigned char input;
    if( read(0, &input, 1) > 0 ){
      // ISTRIP removes bit eight, but other terminals might not.
      *output = input & B7_MASK;
      return 1;
    } else {
      return 0;
    }
  }
}

void machine_write_tty_byte(char output)
{
  write(1, &output, 1);
}

char machine_read_from_file()
{
  return tty_read_from_file;
}

void machine_set_tty_file_name(char *tty_file_name)
{
  tty_file = tty_file_name;
}

char machine_set_read_from_file(char flag)
{
  tty_read_from_file = flag;

  if( flag ){
    tty_fh = fopen(tty_file, "r");
    if( tty_fh == NULL ){
      tty_read_from_file = 0;
    }
  } else {
    if( tty_fh ){
      if( fclose(tty_fh) ){
	tty_read_from_file = 1;
      } else {
	tty_read_from_file = 0;
      }
    }
  }

  return tty_read_from_file;
}
