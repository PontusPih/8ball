#include <stdio.h>
#include "cpu.h"
#include "tty.h"
#include "tty_cpu.h"

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
  ac = pc = cpma = mb = ir = mq = sr = 0;

#include "rimloader.h"  
}

int cpu_process()
{
  pc = (cpma + 1) & B12_MASK;
  mb = mem[cpma];
  ir = mb & 07000;

  if( mb & Z_MASK ){
    cpma = cpma & PAGE_MASK;
  } else {
    cpma = 0;
  }

  cpma = cpma | (mb & WORD_MASK);  

  if( mb & I_MASK ){
    if( cpma >= 010 && cpma <= 017 ){
      mem[cpma] = INC_12BIT(mem[cpma]);
    }
    cpma = mem[cpma];
  }
  
  switch( ir ){
  case JMS:
    mem[cpma] = pc;
    pc = INC_12BIT(cpma);
    break;
  case ISZ:
    mem[cpma] = INC_12BIT(mem[cpma]);
    if( mem[cpma] == 0 ) {
      pc = INC_12BIT(pc);
    }
    break;
  case IOT:
    tty_instr(mb);
    break;
  case DCA:
    mem[cpma] = ac & AC_MASK;
    ac = ac & LINK_MASK;
    break;
  case AND:
    ac = (ac & mem[cpma]) | ( ac & LINK_MASK );
    break;
  case JMP:
    pc = cpma;
    break;
  case TAD:
    ac = (ac + mem[cpma]) & LINK_AC_MASK;
    break;
  case OPR:
    if( ! ( mb & OPR_G2 ) ){
      // Group 1
      if( mb & CLA ){
	ac = ac & LINK_MASK;
      }

      if( mb & CLL ){
	ac = ac & AC_MASK;
      }

      if( mb & CMA ){
	ac = ac ^ AC_MASK;
      }

      if( mb & CML ){
	ac = ac ^ LINK_MASK;
      }

      if( mb & IAC ){
	ac = (ac + 1) & LINK_AC_MASK;
      }

      if( mb & RAR ){
	ac = (ac >> 1) | ((ac & 1) ? LINK_MASK : 0);
	if( mb & BSW ){
	  ac = (ac >> 1) | ((ac & 1) ? LINK_MASK : 0);
	}
      }

      if( mb & RAL ){
	ac = (ac << 1) | ((ac & LINK_MASK) ? 1 : 0);
	if( mb & BSW ){
	  ac = (ac << 1) | ((ac & LINK_MASK) ? 1 : 0);
	}
      }

      if( !( mb & RAR ) && ! ( mb & RAL ) && ( mb & BSW ) ){
	short upper = (ac >> 6) & 077;
	short lower = (ac << 6) & 07700;

	ac = upper | lower | (ac & LINK_MASK);
      }
    } else {
      // Group 2 or 3
      if( ! (mb & OPR_G3 ) ){
	// Group 2

	// OR group
	char do_skip = 0;
	
	if( (mb & SMA) && (ac & 04000) ){
	  do_skip = 1;
	}
	
	if( (mb & SZA) && (ac & B12_MASK) == 0 ){
	  do_skip = 1;
	}
	
	if( (mb & SNL) && (ac & LINK_MASK) ){
	  do_skip = 1;
	}

	if( mb & OPR_AND ){
	  do_skip = !do_skip;
	}
	
	if( do_skip ){
	  pc = (pc + 1) & B12_MASK;
	}
	
	if( mb & CLA ){
	  ac = ac & LINK_MASK;
	}

	if( mb & OSR ){
	  ac = ac | sr;
	}
	
	if( mb & HLT ){
	  return -1;
	}
      } else {
	// Group 3
	if( mb & CLA ){
	  ac = ac & LINK_MASK;
	}

	if( (mb & MQA) && (mb & MQL) ){
	  short tmp = mq;
	  mq = ac & AC_MASK;
	  ac = tmp | (ac & LINK_MASK);
	} else {
	  if( mb & MQA ){
	    ac = ac | mq;
	  }
	  
	  if( mb & MQL ){
	    mq = ac & AC_MASK;
	    ac = ac & LINK_MASK;
	  }
	}
      }
    }
    break;
  default:
    printf( "Not implemented yet!\n");
    return -1;
    break;
  }

  cpma = pc;
  return 1;
}
