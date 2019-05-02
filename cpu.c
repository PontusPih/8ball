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
  ac = pc = cpma = mb = ir = mq = sr = 0;
}

int cpu_process()
{
  return -1;
}
