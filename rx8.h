/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#ifndef _RX8_H_
#define _RX8_H_

// RX8E instructions
#define RXNOP 00 // Just another NOP
#define LCD 01   // Load Command (clear AC)
#define XDR 02   // Transfer Data Register
#define STR 03   // Skip on Transfer Request flag
#define SER 04   // Skip on ERror flag
#define SDN 05   // Skip on DoNe flag
#define INTR 06  // enable INTeRrupts
#define INIT 07  // INITialize RX01 drive

// RX8E interface registers
extern short IR; // Interface Register
extern short TR; // Transfer Request flag
extern short DF; // Done Flag
extern short EF; // Error Flag

// RX8E status bits
extern short online; // Online (1) or offline (0) flag


void rx8_process(short mb);

#endif // _RX8_H_
