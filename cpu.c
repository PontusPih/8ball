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
}

int cpu_process()
{
  return 0;
}
