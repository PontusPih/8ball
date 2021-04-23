/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#include <stdio.h> // for RX and TTY file read
#include <unistd.h>

#include "cpu.h"
#include "tty.h"
#include "rx8.h"
#include "machine.h"

#include "frontend.h"
#include "backend.h"

#define UNUSED(x) (void)(x);

void machine_setup(char *pty_name)
{
#ifdef PTY_CLI
  frontend_setup(pty_name);
#else
  UNUSED(pty_name); // To avoid warning.
  backend_setup();
#endif
}


char machine_run(char single)
{
  unsigned char run_buf[1] = { single ? 'S' : 'R' };
  unsigned char io_buf[2];
  unsigned int io_len = 0;
  char io_queued = 0;
  unsigned char reply_buf[128];
  reply_buf[0] = 'X';
  while( 1 ){
    unsigned char *send_buf = io_queued ? io_buf : run_buf;
#ifdef PTY_CLI
    unsigned int send_len = io_queued ? io_len : 1;
    frontend_dispatch(send_buf, send_len, reply_buf, 1);
#else
    int reply_length;
    backend_dispatch(send_buf, reply_buf, &reply_length);
#endif
    io_queued = io_len = 0;
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
	    io_queued = 1;
	    io_len = 2;
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
      break;
    default:
      return reply_buf[0];
      break;
    }      
  }
  return reply_buf[0];
}


short machine_examine_mem(short addr)
{
#ifdef PTY_CLI
  return frontend_examine_mem(addr);
#else
  return mem[addr];
#endif
}


void machine_deposit_mem(short addr, short val)
{
#ifdef PTY_CLI
  frontend_deposit_mem(addr, val);
#else
  mem[addr] = val;
#endif
}


short machine_operand_addr(short addr, char examine)
{
#ifdef PTY_CLI
  return frontend_operand_addr(addr, examine);
#else
  return operand_addr(addr, examine);
#endif
}


short machine_direct_addr(short addr)
{
#ifdef PTY_CLI
  return frontend_direct_addr(addr);
#else
  return direct_addr(addr);
#endif
}


short machine_examine_reg(register_name_t regname)
{
#ifdef PTY_CLI
  return frontend_examine_reg(regname);
#else
  return backend_examine_deposit_reg(regname, 0, 0);
#endif
}


void machine_deposit_reg(register_name_t regname, short val)
{
#ifdef PTY_CLI
  frontend_deposit_reg(regname, val);
#else
  backend_examine_deposit_reg(regname, val, 1);
#endif
}


void machine_clear_all_bp()
{
#ifdef PTY_CLI
  // TODO Clear all breakpoints
#else
  backend_clear_all_bp();
#endif
}


short machine_examine_bp(short addr)
{
#ifdef PTY_CLI
  return frontend_examine_bp(addr);
#else
  return backend_examine_bp(addr);
#endif
}


void machine_toggle_bp(short addr)
{
#ifdef PTY_CLI
  frontend_toggle_bp(addr);
#else
  backend_toggle_bp(addr);
#endif
}


void machine_set_stop_at(short addr)
{
#ifdef PTY_CLI
  frontend_set_stop_at(addr);
#else
  backend_set_stop_at(addr);
#endif
}


void machine_interrupt()
{
#ifdef PTY_CLI
  frontend_interrupt();
#else
  backend_interrupt();
#endif
}


void machine_cleanup()
{
#ifdef PTY_CLI
  frontend_cleanup();
#endif
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
