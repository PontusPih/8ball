#include "cpu.h"
#include "tty.h"

// CPU registers
short ac;
short pc;
short cpma;
short mb;
short ir;
short mq;
short sr;

// Memory
short mem[MEMSIZE];

// Direct addressing
short direct_addr(short pc){
  return 0;
}

// Indirect addressing and auto index
short indirect_addr(short pc, char examine){
  return 0;
}

void cpu_init(void){
  for(int i=0; i < MEMSIZE; i++){
    mem[i] = 0;
  }
  ac = pc = cpma = mb = ir = mq = sr = tty_tp_flag = tty_kb_flag = 0;
}

int cpu_process()
{
  pc = (cpma + 1) & 07777;
  mb = mem[cpma];
  ir = mb & 07000;

  short operand = 0;
  short operand_addr = 0;
  if( ir <= 05000 ){
    if( ! (mb & Z_MASK) ){
      cpma = cpma & 0177;
    }
    operand_addr = (cpma & 07600) + (mb & 0177);

    if( mb & I_MASK ){
      operand_addr = mem[operand_addr] & 07777;
    }
    operand = mem[operand_addr] & 07777;
  }
  
  switch( ir ) {

  case AND:
    ac = (ac & 010000) | (ac & operand);
    break;
  case TAD:
    ac = (ac + operand) & 017777;
    break;
  case DCA:
    mb = ac & 07777;
    ac = ac & 010000;
    mem[operand_addr] = mb;
    break;
  case JMP:
    cpma = operand_addr;
    return 0;
  case IOT:
    switch( mb & 0770 ) {
    case 040:
      switch( mb & 07 ){
      case TLS:
	tty_tp_buf = ac & 0377;
	tty_initiate_output();
	tty_tp_flag = 0;
	break;
      case TSF:
	if( tty_tp_flag ){
	  pc = INC_PC(pc);
	}
	break;
      }
      break;
    default:
      // NOP
      break;
    }
    break;
  case OPR:
    if( ! (mb & OPR_G2) ){ // Group 1
      if( mb & CLA ){
	ac = ac & 010000;
      }

      if( mb & CLL ){
	ac = ac & 07777;
      }

      if( mb & CMA ) {
	ac = (ac & 010000) | (ac ^ 07777);
      }

      if( mb & CML ){
	ac = (ac ^ 010000) | (ac & 07777);
      }

      if( mb & IAC ){
	ac = (ac + 1) & 017777;
      }

      if( mb & RAR ){
	short l = (ac & 01) ? 010000 : 0;
	ac = (ac >> 1) | l;
	if( mb & BSW ) {
	  l = (ac & 01) ? 010000 : 0;
	  ac = (ac >> 1) | l;
	}
      }

      if( mb & RAL ) {
	short l = (ac & 010000) ? 1 : 0;
	ac = (ac << 1) | l;
	if( mb & BSW ) {
	  l = (ac & 010000) ? 1 : 0;
	  ac = (ac << 1) | l;
	}
      }

      if((mb & BSW)
	 &&
	 (!(mb & RAR) && !(mb & RAL)) ) {
	ac = ((ac & 07700) >> 6) | ((ac & 077) << 6) | (ac & 010000);
      }
    } else {
      if( ! (mb & OPR_G3) ) {
	// We are in group 2 OPR
	if( mb & OPR_AND ){
	  char do_skip = 1;

	  if( mb & SPA && (ac & 04000) ){
	    do_skip = 0;
	  }

	  if( mb & SNA && (ac & 07777) == 0 ){
	    do_skip = 0;
	  }

	  if( mb & SZL && LINK ){
	    do_skip = 0;
	  }

	  if( mb & CLA ){
	    ac = ac & 010000;
	  }

	  if( do_skip ){
	    pc = INC_PC(pc);
	  }

	} else { // Or Group
	  char do_skip = 0;

	  if( mb & SMA && ac & 04000 ){
	    do_skip = 1;
	  }

	  if( mb & SZA && ((ac & 07777) == 0) ){
	    do_skip = 1;
	  }

	  if( mb & SNL && LINK ){
	    do_skip = 1;
	  }

	  if( mb & CLA ){
	    ac = ac & 010000;
	  }

	  if( do_skip ){
	    pc = INC_PC(pc);
	  }

	}
	if( mb & HLT ){
	  cpma = (cpma + 1) & 07777;
	  return -1;
	}

	if( mb & OSR ){
	  ac = (ac & 010000) | sr;
	}
      } else { // We are in group 3
	if( mb & CLA ){
	  ac = ac & 010000;
	}

	if( (mb & MQA) && (mb &MQL) ){
	  short tmp = mq & 07777;
	  mq = ac & 07777;
	  ac = (ac & 010000) | tmp;
	} else {
	  if( mb & MQA ){
	    ac = ac | (mq & 07777);
	  }

	  if( mb & MQL ){
	    mq = ac & 07777;
	    ac = ac & 010000;
	  }
	}
      }
    }
    break;
  }

  cpma = pc;

  return 0;
}
