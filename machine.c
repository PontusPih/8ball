/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#include "cpu.h"
#include "tty.h"
#include "rx8.h"
#include "machine.h"

#ifdef PTY_CLI
#include "frontend.h"
#else
#include "backend.h"
#include <stdio.h> // for RX file read
#endif

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
#ifdef PTY_CLI
  return frontend_run(single);
#else
  return backend_run(single);
#endif
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


void machine_quit()
{
#ifdef PTY_CLI
  frontend_quit();
#endif
}


void machine_interrupt(char wait)
{
#ifdef PTY_CLI
  frontend_interrupt(wait);
#else
  backend_interrupt();
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
