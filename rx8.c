 /*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#include <stdio.h>

#include "cpu.h"
#include "rx8.h"

// RX8E interface registers
short rx_ir = 0; // Interface Register
short rx_tr = 0; // Transfer Request flag
short rx_df = 0; // Done Flag
short rx_ef = 0; // Error Flag

// RX8E status bits
short rx_online = 1; // Online (1) or offline (0) flag. Online means
                     // cable to RX01 drive is connected.
short rx_bit_mode = 1; // Bit Mode.  0 = 12-bit, 1 = 8-bit
short rx_maintenance_mode = 0; // Maintenance mode. 0 = off, !0 = on
short rx_intr_enabled = 0; // RX8E may generate interrupts

static void rx01_LCD(short ac);
static void rx01_XDR();
static void rx01_INIT();

void rx8e_reset()
{
  rx_ir = 0;
  rx_tr = 0;
  rx_df = 0;
  rx_ef = 0;
  rx_online = 1;
  rx_bit_mode = 1;
  rx_maintenance_mode = 0;
  rx_intr_enabled = 0;
  rx01_INIT();
}

void rx8e_check_interrupt()
{
  if( rx_df && rx_intr_enabled ){
    cpu_raise_interrupt(RX_INTR_FLAG);
  } else {
    cpu_lower_interrupt(RX_INTR_FLAG);
  }
}

void rx8e_process(short mb)
{
  switch( mb & IOT_OP_MASK ){
  case RX_NOP: // Just another NOP
    break;
  case RX_LCD: // Load Command (clear AC)
    rx_ir = ac;
    rx01_LCD(rx_ir);
    rx_bit_mode = (rx_ir & RX_MODE_MASK) ? 1 : 0;
    rx_maintenance_mode = (rx_ir & RX_MAINT_MASK) ? 1 : 0;
    if( rx_maintenance_mode ){
      rx_tr = rx_df = rx_ef = 1; // Setting maint mode raises all flags
    }
    ac = (ac & LINK_MASK);
    break;
  case RX_XDR: // Transfer Data Register
    rx01_XDR();
    if( rx_bit_mode ){
      ac |= rx_ir & 0377; // 8-bit mode ORs with AC
    } else {
      ac = rx_ir; // 12-bit mode overwrites
    }
    break;
  case RX_STR: // Skip on Transfer Request flag
    if( rx_tr ){
      pc = INC_PC(pc);
      rx_tr = rx_maintenance_mode;
    }
    break;
  case RX_SER: // Skip on ERror flag
    if( rx_ef ){
      pc = INC_PC(pc);
      rx_ef = rx_maintenance_mode;
    }
    break;
  case RX_SDN: // Skip on DoNe flag
    if( rx_df ){
      pc = INC_PC(pc);
      rx_df = rx_maintenance_mode;
    }
    break;
  case RX_INTR: // enable/disable INTeRrupts
    rx_intr_enabled = ac & 1;
    break;
  case RX_INIT: // INITialize RX01 drive
    rx8e_reset();
    rx01_INIT();
    break;
  default:
    printf("illegal IOT instruction. Device: %o. Operation: %o - RX8\n", (mb & DEV_MASK) >> 3, mb & IOT_OP_MASK );
    break;
  }
  // Setting df through maintenance mode and enabling interrupts
  // will trigger an immediate interrupt.
  rx8e_check_interrupt();
}

// Controller (RX01) bits and registers, one set for each drive.
short RXES[2] = {0}; // RX Error and Status bits
short track[2] = {0}; // Current track, always between 0 and 0114
short sector[2] = {0}; // Current sector, always between 1 and 032
short sector_buffer[2][64] = {0}; // Buffer for read and write
short current_function = 0; // Function being performed, may take
                            // several host instructions
short current_drive = 0; // Drive the current function is performed on
short rx_ready[2] = {0}; // 0 - Door open or no floppy present TODO, load floppy from console
                         // 1 - Door closed and floppy present
short init_delay = 100; // One hundred instruction delay to finish INIT


void rx01_LCD(short ir)
{
  if( ! rx_online ){
    return;
  }
  if( ! rx_df ) {
    // Unless the done flag is low, the drive controller will not
    // accept a new command.
    current_function = (ir & RX_FUNC_MASK) >> 1;
    current_drive = (ir & RX_DRVSEL_MASK) ? 1 : 0;
  }
}

void rx01_XDR()
{
  if( ! rx_online ){
    return;
  }
}

void rx01_INIT()
{
  if( ! rx_online ){
    return;
  }
  current_function = F_INIT;
  current_drive = 0;
  init_delay = 100;
}

void rx01_process()
{
  if( ! rx_online ){
    return;
  }
  if( ! rx_df ){
    switch( current_function ) {
    case F_FILL_BUF:
      rx_df = 1;
      break;
    case F_EMPTY_BUF:
      rx_df = 1;
      break;
    case F_WRT_SECT:
      rx_df = 1;
      break;
    case F_READ_SECT:
      rx_df = 1;
      break;
    case F_INIT:
      {
	if( 0 == init_delay ){
	  rx_df = 1;
	  init_delay = 100;
	  RXES[current_drive] = 04; // TODO define some flags. (04 == init done)
	} else {
	  init_delay--;
	}
	break;
      }
    case F_READ_STAT:
      rx_df = 1;
      break;
    case F_WRT_DD:
      rx_df = 1;
      break;
    case F_READ_ERR:
      rx_df = 1;
      break;
    }

    rx_ir = RXES[current_drive];
  }

  rx8e_check_interrupt();
}
