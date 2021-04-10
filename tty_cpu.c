#include "cpu.h"
#include "tty.h"

void tty_instr(short mb)
{
  switch( (mb & DEV_MASK) >> 3){
  case 03:
    switch( mb & IOT_OP_MASK ){
    case KCF:
      tty_kb_flag = 0;
      break;
    case KSF:
      if( tty_kb_flag ){
	pc = INC_PC(pc);
      }
      break;
    case KCC:
      tty_kb_flag = 0;
      ac &= LINK_MASK;
      break;
    case KRS:
      ac |= (tty_kb_buf & B8_MASK);
      break;
    case KIE:
      tty_dcr = ac & TTY_IE_MASK; // Write IE bit of ac to DCR (SE not supported).
      break;
    case KRB:
      tty_kb_flag = 0;
      ac &= LINK_MASK;
      ac |= (tty_kb_buf & B8_MASK);
      break;
    default:
      // TODO optionally go to console
      //printf("illegal IOT instruction. device 03 - keyboard\n");
      break;
    }
    break;
  case 04: 
    switch( mb & IOT_OP_MASK ){
    case TFL:
      tty_tp_flag = 1;
      break;
    case TSF:
      if( tty_tp_flag ){
	pc = INC_PC(pc);
      }
      break;
    case TCF:
      tty_tp_flag = 0;
      break;
    case TPC:
      tty_tp_buf = (ac & B7_MASK); // emulate ASR with 7M1
      tty_initiate_output();
      break;
    case TSK:
      if( tty_tp_flag || tty_kb_flag ){
	pc = INC_PC(pc);
      }
      break;
    case TLS:
      tty_tp_buf = (ac & B7_MASK); // emulate ASR with 7M1
      tty_initiate_output();
      tty_tp_flag = 0;
      break;
    default:
      // TODO optionally go to console
      //printf("illegal IOT instruction. device 04 - TTY output\n");
      break;
    }
    break;
  }
}
