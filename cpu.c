#include "cpu.h"
#include "tty.h"

// CPU registers
short ac;
short l;
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
  ac = l = pc = cpma = mb = ir = mq = sr = 0;
}

int cpu_process()
{
  pc = (cpma + 1) & 07777;
  mb = mem[cpma];
  ir = mb & 07000;
  
  switch( ir ) {

  case OPR:
    if( mb & OPR_G2 ){
      if( mb & OPR_AND ){
	char spa_skip = 1;
	char sna_skip = 1;
	char szl_skip = 1;

	if( mb & SPA // och så vidare
      }
      if( mb & HLT ){
	return -1;
      }
    }
    break;
  }

  cpma = (cpma + 1) & 07777;

  return 0;
}
