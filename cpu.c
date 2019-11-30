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

void cpu_init(void){
  for(int i=0; i < MEMSIZE; i++){
    mem[i] = 0;
  }
#include "rimloader.h"
  ac = pc = cpma = mb = ir = mq = sr = 0;
}

int cpu_process()
{
  pc = (cpma + 1) & B12_MASK;
  mb = mem[cpma];
  ir = mb & 07000;

  short operand_addr = 0;
  short operand = 0;

  if( ir <= JMP ){

    if( ! (mb & Z_MASK ) ){
      cpma = cpma & 0177;
    }
    
    operand_addr = (cpma & 07600) | (mb & 0177);

    if( mb & I_MASK ){
      if( operand_addr >= 010 && operand_addr <= 017 ){
	mem[operand_addr] = (mem[operand_addr] + 1) & B12_MASK;
      }
      operand_addr = mem[operand_addr] & B12_MASK;
    }
    
    operand = mem[operand_addr] & B12_MASK;
    
  }

  switch( ir ){
  case AND:
    ac = (ac & LINK_MASK) | (ac & operand);
    break;
  case TAD:
    ac = (ac + operand) & LINK_AC_MASK;
    break;
  case ISZ:
    operand = (operand + 1) & B12_MASK;
    mem[operand_addr] = operand;
    if( operand == 0 ){
      pc = INC_PC(pc);
    }
    break;
  case DCA:
    mb = ac & B12_MASK;
    ac = ac & LINK_MASK;
    mem[operand_addr] = mb;
    break;
  case JMS:
    mb = pc;
    pc = (operand_addr + 1) & B12_MASK;
    mem[operand_addr] = mb;
    break;
  case JMP:
    cpma = operand_addr;
    return 0;
  case IOT:
    switch( mb & 0770 ){
    case 030:
      switch( mb & 07 ){
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
	ac = ac & 010000;
	break;
      case KRS:
	ac = ac | (tty_kb_buf & 0377);
	break;
      case KRB:
	tty_kb_flag = 0;
	ac = ac & 010000;
	ac = ac | (tty_kb_buf & 0377);
	break;
      default:
	break;
      }
      break;
    case 040:
      switch( mb & 07 ){
      case TLS:
	tty_tp_buf = ac & 0177;
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
    }
    break;
  case OPR:
    if( ! ( mb & OPR_G2) ){ // Group 1
      if( mb & CLA ){
	ac = ac & LINK_MASK;
      }

      if( mb & CLL ){
	ac = ac & B12_MASK;
      }

      if( mb & CMA ){
	ac = (ac ^ B12_MASK);
      }

      if( mb & CML ){
	ac = ac ^ LINK_MASK;
      }

      if( mb & IAC ){
	ac = (ac + 1) & LINK_AC_MASK;
      }

      if( mb & RAR ){
	short l = (ac & 01) ? 010000 : 0;
	ac = (ac >> 1) | l;
	if( mb & BSW ){
	  l = (ac & 01) ? 010000 : 0;
	  ac = (ac >> 1) | l;
	}
      }

      if( mb & RAL ){
	short l = (ac & 010000) ? 1 : 0;
	ac = (ac << 1) | l;
	if( mb & BSW ){
	  l = (ac & 010000) ? 1 : 0;
	  ac = (ac << 1) | l;
	}
	ac = ac & LINK_AC_MASK;
      }

      if( (mb & BSW)
	  && (!(mb & RAR) && !(mb & RAL)) ){
	ac = ((ac & 07700) >> 6) | ((ac & 077) << 6) | (ac & 010000);
      }
      
    } else { // Group 2
      if( ! ( mb & OPR_G3 ) ){ // Group 2
	if( mb & OPR_AND ) { // And group
	  char do_skip = 1;

	  if( mb & SPA && (ac & 04000) ){
	    do_skip = 0;
	  }

	  if( mb & SNA && (ac & B12_MASK) == 0 ){
	    do_skip = 0;
	  }

	  if( mb & SZL && (ac & 010000) ){
	    do_skip = 0;
	  }

	  if( mb & CLA ){
	    ac = ac & 010000;
	  }

	  if( do_skip ){
	    pc = INC_PC(pc);
	  }
	  
	} else { // Or group
	  char do_skip = 0;

	  if( mb & SMA && (ac & 04000) ){
	    do_skip = 1;
	  }

	  if( mb & SZA && (ac & B12_MASK) == 0 ){
	    do_skip = 1;
	  }

	  if( mb & SNL && (ac & 010000) ){
	    do_skip = 1;
	  }

	  if( mb & CLA ){
	    ac = ac & 010000;
	  }

	  if( do_skip ){
	    pc = INC_PC(pc);
	  }
	  	  
	}

	if( mb & OSR ){
	  ac = ac | sr;
	}
	
	if( mb & HLT ){
	  cpma = (cpma + 1) & B12_MASK;
	  return -1;
	}

	
      } else { // Group 3
	if( mb & CLA ){
	  ac = ac & LINK_MASK;
	}

	if( (mb & MQA) && (mb & MQL) ){
	  short tmp = mq & B12_MASK;
	  mq = ac & B12_MASK;
	  ac = (ac & LINK_MASK) | tmp;
	} else {
	  if( mb & MQA) {
	    ac = ac | (mq & 07777);
	  }

	  if( mb & MQL ){
	    mq = ac & B12_MASK;
	    ac = ac & LINK_MASK;
	  }
	}
      }
    }
    break;
  default:
    return -1;
  }
  
  cpma = pc;
  return 0;
}
