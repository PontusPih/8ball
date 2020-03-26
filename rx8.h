/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#ifndef _RX8_H_
#define _RX8_H_

// RX8E instructions
#define RX_NOP 00 // Just another NOP
#define RX_LCD 01   // Load Command (clear AC)
#define RX_XDR 02   // Transfer Data Register
#define RX_STR 03   // Skip on Transfer Request flag
#define RX_SER 04   // Skip on ERror flag
#define RX_SDN 05   // Skip on DoNe flag
#define RX_INTR 06  // enable INTeRrupts
#define RX_INIT 07  // INITialize RX01 drive

// RX8E interface registers
extern short rx_ir; // Interface Register
extern short rx_tr; // Transfer Request flag
extern short rx_df; // Done Flag
extern short rx_ef; // Error Flag

// RX8E status bits
extern short rx_online; // Online (1) or offline (0) flag Online means
                        // cable to RX01 drive is connected.
extern short rx_bit_mode; // Bit Mode.  0 = 12-bit, !0 = 8-bit
extern short rx_maintenance_mode; // Maintenance mode. 0 = off, !0 = on
extern short rx_intr_enabled; // RX8E may generate interrupts

void rx8_reset();
void rx8_process(short mb);

#endif // _RX8_H_
