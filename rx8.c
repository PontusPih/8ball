 /*
  Copyright (c) 2020 Pontus Pihlgren <pontus.pihlgren@gmail.com>
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
short rx_run = 0; // Run flag, indicates a function should start

#define INTR_MASK 0b1

static void rx01_LCD(short ac);
static void rx01_XDR();
static void rx01_INIT();

void rx8e_reset()
{
  rx_ir = 0;
  rx_tr = 0;
  rx_df = 0;
  rx_ef = 0;
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
    rx_ir = ac & B12_MASK; // Load rx_ir parallel from OMNIBUS
    rx01_LCD(rx_ir);
    rx_bit_mode = (rx_ir & RX_MODE_MASK) ? 1 : 0;
    rx_maintenance_mode = (rx_ir & RX_MAINT_MASK) ? 1 : 0;
    if( rx_maintenance_mode ){
      rx_tr = rx_df = rx_ef = 1; // Setting maint mode raises all flags
      rx_run = 0; // setting rx_df to 1 forces rx_run to 0
    }
    ac = (ac & LINK_MASK);
    break;
  case RX_XDR: // Transfer Data Register
    rx01_XDR();
    if( rx_bit_mode ){
      ac |= rx_ir & 0377; // 8-bit mode ORs with AC
    } else {
      ac = (ac & LINK_MASK) | (rx_ir & B12_MASK) ; // 12-bit mode overwrites
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
    rx_intr_enabled = ac & INTR_MASK;
    break;
  case RX_INIT: // INITialize RX01 drive
    rx8e_reset();
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
unsigned char sector_buffer[2][128] = {0}; // Buffer for read and write

short current_function = -1; // Function being performed, may take
                             // several host instructions
short current_drive = 0; // Drive the current function is performed on
short rx_ready[2] = {0}; // 0 - Door open or no floppy present TODO, load floppy from console
                         // 1 - Door closed and floppy present
#define RX_INIT_DELAY 30
short init_delay = RX_INIT_DELAY; // default delay to finish INIT

#define RX_INIT_DONE 04

void rx01_LCD(short ir)
{
  if( ! rx_online ){
    return;
  }
  if( ! rx_run && ! rx_df ) {
    // Unless the done flag is low, the drive controller will not
    // accept a new command.
    current_function = (ir & RX_FUNC_MASK) >> 1;
    current_drive = (ir & RX_DRVSEL_MASK) ? 1 : 0;
    rx_run = 1;
  }
}

void rx01_XDR()
{
  if( ! rx_online ){
    return;
  }
  if( rx_maintenance_mode ){ // Don't set rx_run flag
    return;
  }
  if( ! rx_df && current_function >= 0){
    if( current_function == F_FILL_BUF ){
      rx_ir = ac & AC_MASK;
    }
    rx_run = 1; // Continue current_function
  }
}

void rx01_INIT()
{
  if( ! rx_online ){
    return;
  }
  if( !rx_run && ! rx_df ) { // TODO init forces df low
    current_function = F_INIT;
    current_drive = 0;
    init_delay = RX_INIT_DELAY;
    rx_df = 0;
    rx_run = 1;
  }
}

void rx01_process()
{
  if( ! rx_online ){
    return;
  }
  if( rx_run && ! rx_df && current_function >= 0){
    printf("Func %o delay %d rx_run %d rx_df %o rx_bit_mode %o maint %o\n", current_function, init_delay, rx_run, rx_df, rx_bit_mode, rx_maintenance_mode);

    switch( current_function ) {
    case F_FILL_BUF:
      if( 0 == rx_tr ) {
	static char cur = -1;
	if( -1 == cur ){ // Start a sector transfer from the RX8E
	  cur = rx_bit_mode ? 127 : 63;
	} else { // transfer one byte or word
	  if( rx_bit_mode ){ // 8 bit transfer
	    sector_buffer[current_drive][127 - cur] = (char) rx_ir & 0377;
	  } else { // 12 bit transfer
	    sector_buffer[current_drive][127 - 2*cur - 1] = (char) ((rx_ir & 07400) >> 8);
	    sector_buffer[current_drive][127 - 2*cur] = (char) rx_ir & 0377;
	  }
	  cur --;
	}

	if( -1 == cur ){
	  // If cur reached -1 again, one whole sector has been transferred
	  rx_df = 1;
	  rx_tr = 0; // No transfer request after last byte/word
	  rx_ir = RXES[current_drive] & B12_MASK;
	  current_function = -1;
	} else {
	  rx_tr = 1;
	}
	rx_run = 0;
      }
      break;
    case F_EMPTY_BUF:
      if( 0 == rx_tr ){
	static char state = 0;
	static char cur = -1;
	switch( state ){
	case 0: // Start a sector transfer to the RX8E
	  cur = rx_bit_mode ? 127 : 63;
	  state = 1;
	  // Fall through to first transfer
	case 1: // Transfer sector data
	  if( rx_bit_mode ){
	    rx_ir = sector_buffer[current_drive][127 - cur];
	  } else {
	    rx_ir = sector_buffer[current_drive][127 - 2*cur - 1];
	    rx_ir = rx_ir << 8 | sector_buffer[current_drive][127 - 2*cur];
	  }
	  cur --;
	  if( -1 == cur ) {
	    state = 2; // Last bit of data to transfer
	  }
	  rx_tr = 1;
	  break;
	case 2: // One whole sector has been transferred
	  rx_df = 1;
	  rx_tr = 0; // No transfer request after last byte/word
	  rx_ir = RXES[current_drive] & B12_MASK;
	  current_function = -1;
	  state = 0;
	  break;
	}
	rx_run = 0;
      }
      break;
    case F_WRT_SECT:
      rx_df = 1;
      rx_run = 0;
      rx_ir = RXES[current_drive] & B12_MASK;
      current_function = -1;
      break;
    case F_READ_SECT:
      rx_df = 1;
      rx_run = 0;
      rx_ir = RXES[current_drive] & B12_MASK;
      current_function = -1;
      break;
    case F_INIT:
      if( 0 == init_delay ){
	rx_run = 0;
	rx_df = 1;
	init_delay = RX_INIT_DELAY;
	RXES[current_drive] = RX_INIT_DONE;
	current_function = -1;
      } else {
	init_delay--;
      }
      rx_ir = RXES[current_drive] & B12_MASK;
      break;
    case F_READ_STAT: // Requires one or two revolutions. about 250ms in total
      rx_df = 1;
      rx_run = 0;
      // The status register is only 8 bits shifts from controller to rx8e
      rx_ir = (rx_ir << 8) & B12_MASK; // No internal errors implemented yet :)
      current_function = -1;
      break;
    case F_WRT_DD:
      rx_df = 1;
      rx_run = 0;
      rx_ir = RXES[current_drive] & B12_MASK;
      current_function = -1;
      break;
    case F_READ_ERR:
      rx_df = 1;
      rx_run = 0;
      // The error register is only 8 bits shifts from controller to rx8e
      rx_ir = (rx_ir << 8) & B12_MASK; // No internal errors implemented yet :)
      current_function = -1;
      break;
    }
  }

  rx8e_check_interrupt();
}
