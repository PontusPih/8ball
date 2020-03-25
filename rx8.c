 /*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#include <stdio.h>

#include "cpu.h"
#include "rx8.h"

#define RX_FUNC_MASK 016
#define RX_DRVSEL_MASK 020
#define RX_MODE_MASK 060
#define RX_MAINT_MASK 0200

// RX8E interface registers
short rx_ir = 0; // Interface Register
short rx_tr = 0; // Transfer Request flag
short tr_df = 0; // Done Flag
short rx_ef = 0; // Error Flag

// RX8E status bits
short online = 1; // Online (1) or offline (0) flag. Online means
                  // cable to RX01 drive is connected.
short bit_mode = 1; // Bit Mode.  0 = 8-bit, !0 = 12-bit
short maintenance_mode = 0; // Maintenance mode. 0 = off, !0 = on
short intr_enabled = 0; // RX8E may generate interrupts

static void controller_LCD(short ac);
static short controller_XDR();

void rx8_process(short mb)
{
  switch( mb & IOT_OP_MASK ){
  case RX_NOP: // Just another NOP
    break;
  case RX_LCD: // Load Command (clear AC)
    rx_ir = ac;
    controller_LCD(rx_ir);
    bit_mode = (rx_ir & RX_MODE_MASK);
    maintenance_mode = (rx_ir & RX_MAINT_MASK) ? 1 : 0;
    if( maintenance_mode ){
      rx_tr = tr_df = rx_ef = 1; // Setting maint mode raises all flags
    }
    ac = (ac & LINK_MASK);
    break;
  case RX_XDR: // Transfer Data Register
    if( bit_mode ){
      ac = rx_ir; // 12-bit mode overwrites
    } else {
      ac |= rx_ir & 0377; // 8-bit mode ORs with AC
    }
    break;
  case RX_STR: // Skip on Transfer Request flag
    if( rx_tr ){
      pc = INC_PC(pc);
    }
    break;
  case RX_SER: // Skip on ERror flag
    if( rx_ef ){
      pc = INC_PC(pc);
      rx_ef = maintenance_mode;
    }
    break;
  case RX_SDN: // Skip on DoNe flag
    if( tr_df ){
      pc = INC_PC(pc);
      tr_df = maintenance_mode;;
    }
    break;
  case RX_INTR: // enable/disable INTeRrupts
    intr_enabled = ac & 1;
    break;
  case RX_INIT: // INITialize RX01 drive
    break;
  default:
    printf("illegal IOT instruction. Device: %o. Operation: %o - RX8\n", (mb & DEV_MASK) >> 3, mb & IOT_OP_MASK );
    break;
  }
}

// Controller (RX01) bits and registers, one set for each drive.
short RXES[2] = {0}; // RX Error and Status bits
short track[2] = {0}; // Current track, always between 0 and 0114
short sector[2] = {0}; // Current sector, always between 1 and 032
short sector_buffer[2][64] = {0}; // Buffer for read and write
short current_function = -1; // Function being performed, may take
                             // several host instructions
short current_drive = 0; // Drive the current function is performed on-

void controller_LCD(short ir)
{
  current_function = (ir & RX_FUNC_MASK) >> 1;
  current_drive = (ir & RX_DRVSEL_MASK) ? 1 : 0;
}

short controller_XDR()
{
  return 0;
}
