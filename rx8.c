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

#define RX_INTR_MASK 0b1

static void rx01_LCD();
static void rx01_XDR(short data);
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
  cpu_lower_interrupt(RX_INTR_FLAG);
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
    rx01_LCD();
    rx_bit_mode = (rx_ir & RX_MODE_MASK) ? 1 : 0;
    rx_maintenance_mode = (rx_ir & RX_MAINT_MASK) ? 1 : 0;
    if( rx_maintenance_mode ){
      rx_tr = rx_df = rx_ef = 1; // Setting maint mode raises all flags
      rx_run = 0; // setting rx_df to 1 forces rx_run to 0
    }
    ac = (ac & LINK_MASK);
    break;
  case RX_XDR: // Transfer Data Register
    rx01_XDR(ac & AC_MASK);
    // It is safe to always set AC here. It will remain unchanged in
    // AC -> IR transfers because AC and IR are equal then.
    if( rx_bit_mode ){
      ac |= rx_ir & B8_MASK; // 8-bit mode ORs with AC
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
    rx_intr_enabled = ac & RX_INTR_MASK;
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
short RXER[2] = {0}; // RX Error register
unsigned short RXTA[2] = {0}; // Current track, always between 0 and 0114
unsigned short RXSA[2] = {0}; // Current sector, always between 1 and 032
unsigned char sector_buffer[2][128] = {0}; // Buffer for read and write

short current_function = -1; // Function being performed, may take
                             // several host instructions
short current_drive = 0; // Drive the current function is performed on
short rx_ready[2] = {0}; // 0 - Door open or no floppy present TODO, load floppy from console
                         // 1 - Door closed and floppy present
#define RX_DELAY 30
#define RX_INIT_DONE 04
#define RX_DRIVE_RDY 0200
#define RX_DD_MARK 0100

unsigned char data[2][77][26][128] = {0}; // Floppy data;
unsigned char dd_mark[2][77][26] = {0}; // Deleted Data Mark;

void rx01_LCD()
{
  if( ! rx_online ){
    return;
  }
  if( ! rx_run && ! rx_df ) {
    // Unless the done flag is low, the drive controller will not
    // accept a new command.
    current_function = (rx_ir & RX_FUNC_MASK) >> 1;
    current_drive = (rx_ir & RX_DRVSEL_MASK) ? 1 : 0;
    rx_run = 1;
  }
}

void rx01_XDR(short data)
{
  if( rx_online && ! rx_maintenance_mode && ! rx_df && current_function >= 0){
    // Drive is online, not in maintenance mode and the current function is not done
    rx_run = 1; // Assume current_function will continue
    switch( current_function ) {
    case F_FILL_BUF:
    case F_READ_SECT:
    case F_WRT_SECT:
    case F_WRT_DD:
      rx_ir = data;
      rx_run = 1; // Continue current_function
      break;
    case F_EMPTY_BUF:
      rx_run = 1; // Continue current_function
      break;
    default:
      rx_run = 0; // No need to continue current_function
    }
  }
}

void rx01_INIT()
{
  if( ! rx_online ){
    return;
  }
  if( current_function != F_INIT ) {
    current_function = F_INIT;
    current_drive = 0;
    rx_run = 1;
  }
}

void rx01_fill(short drive, char *buf)
{
  int ptr = 0;
  for(int i = 0;i < 77; i++){
    for(int j = 0;j < 26; j++){
      for(int k = 0; k < 128; k++){
	data[drive][i][j][k] = buf[ptr++];
      }
    }
  }
}

void rx01_process()
{
  if( ! rx_online ){
    return;
  }
  if( rx_run && ! rx_df && current_function >= 0){
    // extern char trace_instruction;
    static char delay = RX_DELAY;

    /*
    if( trace_instruction && (delay == RX_DELAY || delay == 0) ){
      printf("Func %o delay=%o ir=%o bit_mode=%o maint=%o drv=%o RXES=%o RXER=%o RXTA=%o RXSA=%o\n", current_function, delay, rx_ir, rx_bit_mode, rx_maintenance_mode, current_drive, RXES[current_drive], RXER[current_drive], RXTA[current_drive], RXSA[current_drive]);
    }
    */

    rx_run = 0; // Stop the RX01 until more data is available from CPU
                // The delayed functions will set rx_run = 1 until
                // a suitable number of instructions has passed.

    switch( current_function ) {
    case F_FILL_BUF:
      if( 0 == rx_tr ) {
	static char state = 0;
	static char words = -1; // Number of words left to transfer
	static int ptr = 0;     // Pointer to byte where next word starts
	rx_tr = 1; // Expect more data
	switch( state ){
	case 0: // Start a sector transfer from the RX8E
	  words = rx_bit_mode ? 127 : 63;
	  ptr = 0;
	  state = 1;
	  break;
	case 1:
	  if( rx_bit_mode ){ // Transfer one byte
	    sector_buffer[current_drive][127 - words] = (char) rx_ir & B8_MASK;
	  } else { // Transfer one word
            // Byte index:    0   1   2   3   4   5   6   7   8...
            // Word layout: [aa][ab][bb][cc][cd][dd][ee][ef][ff]...
	    if( (63 - words) & 1 ){ // Odd word address
	      sector_buffer[current_drive][ptr] &= (B4_MASK << 4); // Keep top 4 bits
	      sector_buffer[current_drive][ptr] |= (char) ((rx_ir >> 8) & B4_MASK); // Set new bottom 4 bits
	      sector_buffer[current_drive][ptr+1] = (char) rx_ir & B8_MASK;
	      ptr += 2;
	    } else { // Even word address
	      sector_buffer[current_drive][ptr] = (rx_ir >> 4) & B8_MASK;
	      sector_buffer[current_drive][ptr+1] &= (B4_MASK); // Keep bottom 4 bits
	      sector_buffer[current_drive][ptr+1] |= (rx_ir & B4_MASK) << 4; // Set new top 4 bits
	      ptr ++;
	    }
	  }

	  words --;
	  if( -1 == words){ // One whole sector has been transferred
	    rx_df = 1;
	    rx_tr = 0; // No transfer request after last byte/word
	    rx_ir = RXES[current_drive] & B8_MASK;
	    current_function = -1;
	    state = 0;
	  }
	  break;
	}
      }
      break;
    case F_EMPTY_BUF:
      if( 0 == rx_tr ){
	static char state = 0;
	static char words = -1; // Number of words left to transfer
	static int ptr = 0;     // Pointer to byte where next word starts
	switch( state ){
	case 0: // Start a sector transfer to the RX8E
	  words = rx_bit_mode ? 127 : 63;
	  ptr = 0;
	  state = 1;
	  __attribute__ ((fallthrough)); // Fall through to first transfer
	case 1: // Transfer sector data
	  if( rx_bit_mode ){ // Transfer one byte
	    rx_ir = sector_buffer[current_drive][127 - words] & B8_MASK;
	  } else { // Transfer one word
            // Byte index:    0   1   2   3   4   5   6   7   8...
            // Word layout: [aa][ab][bb][cc][cd][dd][ee][ef][ff]...
	    if( (63 - words) & 1 ){ // Odd word address
	      rx_ir = sector_buffer[current_drive][ptr] & B4_MASK;
	      rx_ir = rx_ir << 8 | sector_buffer[current_drive][ptr+1];
	      ptr += 2;
	    } else { // Even word address
	      rx_ir = sector_buffer[current_drive][ptr] << 4;
	      rx_ir |= (sector_buffer[current_drive][ptr+1] >> 4) & B4_MASK;
	      ptr ++;
	    }
	  }
	  words --;
	  if( -1 == words ) {
	    state = 2; // Last bit of data transferred
	  }
	  rx_tr = 1;
	  break;
	case 2: // One whole sector has been transferred
	  rx_df = 1;
	  rx_tr = 0; // No transfer request after last byte/word
	  rx_ir = RXES[current_drive] & B8_MASK;
	  current_function = -1;
	  state = 0;
	  break;
	}
      }
      break;
    case F_WRT_SECT:
    case F_READ_SECT:
    case F_WRT_DD:
      if( 0 == rx_tr ){
	static char state = 0;
	switch(state){
	case 0:
	  RXES[current_drive] = 0; // clear RXES bit 4,5,10 and 11
	  RXER[current_drive] = 0;
	  rx_tr = 1; // Request sector address
	  state = 1; // Wait for sector address
	  break;
	case 1:
	  RXSA[current_drive] = rx_ir & 0177;
	  rx_tr = 1; // Request track address
	  state = 2; // Wait for track address
	  break;
	case 2:
	  if( 0 == delay-- ){
	    state = 3; // Delay done, process transfered data
	    delay = RX_DELAY;
	  }
	  rx_run = 1; // Force delay processing
	  break;
	case 3:
	  RXTA[current_drive] = rx_ir & 0377;
	  int d = current_drive;
	  unsigned char dd = (F_WRT_DD == current_function) ? RX_DD_MARK : 0;
	  unsigned int t = RXTA[d];
	  unsigned int s = RXSA[d];

	  if( t > 0114 ){
	    rx_ef = 1;
	    RXER[current_drive] = 0040; // Tried to access track greater than 77
	  } else if( s < 1 || s > 032 ){
	    rx_ef = 1;
	    RXER[current_drive] = 0070; // Desired sector not found after two revolutions
	  } else { // Track and Sector number ok
	    for(int i=0;i<128;i++){
	      if( F_READ_SECT == current_function ){
		sector_buffer[d][i] = data[d][t][s-1][i];
	      } else { // WRT_SECT and WRT_DD
		data[d][t][s-1][i] = sector_buffer[d][i];
	      }
	    }
	    if( F_READ_SECT == current_function ){
	      dd = dd_mark[d][t][s-1]; // If READ, get DD mark from disk
	    } else {
	      dd_mark[d][t][s-1] = dd; // If F_WRT_DD, set a mark on disk
	    }
	  }
	  rx_df = 1;
	  // dd will be set by F_WRT_DD even for invalid track and sector
	  rx_ir = (RXES[current_drive] & B8_MASK) | dd;
	  current_function = -1;
	  state = 0;
	  break;
	}
      }
      break;
    case F_NOOP:
      current_function = -1;
      rx_df = 1;
      break;
    case F_INIT:
      {
	if( 0 == delay ){
	  rx_run = 0;
	  rx_df = 1;
	  delay = RX_DELAY;
	  RXES[0] = RX_INIT_DONE | (rx_ready[0] ? RX_DRIVE_RDY : 0);
	  RXES[1] = RX_INIT_DONE | (rx_ready[1] ? RX_DRIVE_RDY : 0);
	  RXER[0] = RXER[1] = 0;
	  rx_ir = RXES[0] & B12_MASK;
	  current_function = -1;
	  for(int i=0;i<128;i++){
	    sector_buffer[0][i] = data[0][1][0][i];
	  }
	} else {
	  delay--;
	  rx_run = 1; // Force delay processing
	}
      }
      break;
    case F_READ_STAT: // Requires one or two revolutions. about 250ms in total
      rx_df = 1;
      // The status register is only 8 bits shifted from controller to rx8e
      // RX_INIT_DONE flag is explicitly reset here
      RXES[current_drive] &= ~RX_INIT_DONE;
      rx_ir = ((rx_ir << 8) & B12_MASK) | RXES[current_drive];
      current_function = -1;
      // Maintenance manual mentions that F_READ_ERR should be done
      // before F_READ_STAT because F_READ_STAT inevitably modifies
      // RXES. No test seems to depend on this.
      break;
    case F_READ_ERR:
      rx_df = 1;
      // The error register is only 8 bits shifted from controller to rx8e
      rx_ir = ((rx_ir << 8) & B12_MASK) | (RXER[current_drive] & B8_MASK);
      current_function = -1;
      break;
    }
  }

  rx8e_check_interrupt();
}
