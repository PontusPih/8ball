/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#ifndef _MACHINE_H_
#define _MACHINE_H_

short machine_examine_mem(short addr);
void machine_deposit_mem(short addr, short val);
short machine_operand_addr(short addr, char examine);
short machine_direct_addr(short addr);
short machine_examine_reg(register_name_t regname);
void machine_deposit_reg(register_name_t regname, short val);
void machine_clear_all_bp(void);
short machine_examine_bp(short addr);
void machine_toggle_bp(short addr);
void machine_set_stop_at(short addr);
void machine_interrupt();
void machine_srv();

void machine_setup(char *pty_name);
char machine_run(char single);
void machine_cleanup();

void machine_mount_rx_image(short drive, char *filename);

char machine_read_tty_byte(char *output);
void machine_write_tty_byte(char output);
char machine_read_from_file();
void machine_set_tty_file_name(char *tty_file_name);
char machine_set_read_from_file(char flag);

#endif // _MACHINE_H_
