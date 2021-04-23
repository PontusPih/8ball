/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#ifndef _FRONTEND_H_
#define _FRONTEND_H_

void frontend_setup(char *pty_name);
short frontend_examine_mem(short addr);
void frontend_deposit_mem(short addr, short val);
short frontend_operand_addr(short addr, char examine);
short frontend_direct_addr(short addr);
void frontend_deposit_reg(register_name_t reg, short val);
short frontend_examine_reg(register_name_t reg);
short frontend_examine_bp(short addr);
void frontend_toggle_bp(short addr);
void frontend_set_stop_at(short addr);
void frontend_cleanup();
void frontend_interrupt();
void frontend_dispatch(unsigned char *send_buf, int send_length, unsigned char *reply_buf, char expect_reply);

#endif // _FRONTEND_H_
