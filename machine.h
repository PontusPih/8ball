/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#ifndef _MACHINE_H_
#define _MACHINE_H_

typedef enum register_name {
  AC,
  PC,
  MQ,
  DF,
  IB,
  UB,
  UF,
  SF,
  SR,
  ION_FLAG,
  ION_DELAY,
  INTR_INHIBIT,
  INTR,
  RTF_DELAY,
  TTY_KB_BUF,
  TTY_KB_FLAG,
  TTY_TP_BUF,
  TTY_TP_FLAG,
  TTY_DCR,
  RX_IR,
  RX_TR,
  RX_DF,
  RX_EF,
  RX_ONLINE,
  RX_BIT_MODE,
  RX_MAINTENANCE_MODE,
  RX_INTR_ENABLED,
  RX_RUN,
  RX_FUNCTION,
  RX_READY_0,
  RX_READY_1
} register_name_t;

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

void machine_mount_rx_image(short drive, char *filename);

char machine_read_tty_byte(char *output);
void machine_write_tty_byte(char output);
char machine_read_from_file();
void machine_set_tty_file_name(char *tty_file_name);
char machine_set_read_from_file(char flag);

#endif // _MACHINE_H_
