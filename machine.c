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


char read_tty_byte(char *output)
{
#ifdef PTY_SRV
  unsigned char buf[2] = { 'T', 'R' };
  send_cmd(ptm, buf,2); // TTY wants to Read
  unsigned char *rbuf;
  if(recv_cmd(ptm, &rbuf) > 0 ){
    *output = rbuf[1];
    return rbuf[0];
  } else {
    return -1;
  }
#else
  return console_read_tty_byte(output);
#endif
}


void write_tty_byte(char output)
{
#ifdef PTY_SRV
  unsigned char buf[3] = { 'T', 'W', output };
  send_cmd(ptm, buf,3); // TTY wants to Write
#else
  console_write_tty_byte(output);
#endif
}

char machine_run(char single)
{
#ifdef PTY_CLI
  frontend_run(single);
#else
  backend_run(single);
#endif
}


short machine_examine_mem(short addr)
{
#ifdef PTY_CLI
  return frontend_examine_mem(addr)
#else
  return mem[addr];
#endif
}


void machine_deposit_mem(short addr, short val)
{
#ifdef PTY_CLI
  frontend_deposit_mem(short addr, short val);
#else
  mem[addr] = val;
#endif
}


short machine_operand_addr(short addr, char examine)
{
#ifdef PTY_CLI
  return frontend_deposit_mem(addr, examine)
#else
  return operand_addr(addr, examine);
#endif
}


short machine_direct_addr(short addr)
{
#ifdef PTY_CLI
  frontend_direct_addr(addr);
#else
  return direct_addr(addr);
#endif
}


short machine_examine_reg(register_name_t regname)
{
#ifdef PTY_CLI
  return frontend_examine_reg(reg);
#else
  return backend_examine_deposit_reg(regname, 0, 0);
#endif
}


void machine_deposit_reg(register_name_t regname, short val)
{
#ifdef PTY_CLI
  frontend_deposit_reg(reg, val);
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
  return frontend_examine_bp();
#else
  return backend_examine_bp();
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


short machine_examine_trace()
{
#ifdef PTY_CLI
  return frontend_examine_trace();
#else
  return backend_examine_trace();
#endif
}


void machine_toggle_trace()
{
#ifdef PTY_CLI
  frontend_toggle_trace();
#else
  backend_toggle_trace();
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


void machine_interrupt()
{
#ifdef PTY_CLI
  frontend_interrupt();
#else
  backend_interrupt();
#endif
}


void machine_halt()
{
#ifdef PTY_CLI
  frontend_interrupt();
#endif
}


void machine_mount_rx_image(short drive, char *filename)
{
#ifdef SERVER_BUILD
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
