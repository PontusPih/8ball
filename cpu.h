/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#ifndef _CPU_H_
#define _CPU_H_

// TODO implement "clear" command that initializes these variables,
// just like the clear switch on a real front panel.
// CPU registers
extern short pc; // Program Counter (and Instruction Field, if)
extern short ac; // Acumulator
extern short mq; // Multiplier Quotient
extern short sr; // Switch Registers, 1 is switch up
extern short ion; // Interrupt enable flipflop
extern short ion_delay; //ion will be set after next fetch
extern short intr; // Interrupt requested flags
// Memory extension registers
extern short ib; // Instruction buffer
extern short sf; // save field
extern short df; // data field
extern short intr_inhibit; // interrupt inhibit flag
// Time share registers
extern short uf; // User Field
extern short ub; // User Buffer
// TODO remove rtf_delay
extern short rtf_delay; //ion will be set after next fetch
// TODO add F D E state bits

void cpu_init(void);
int cpu_process(void);
short direct_addr(short pc);
short operand_addr(short pc, char examine);
short mem_read(short addr);
void mem_write(short addr, short value);
void cpu_raise_interrupt(short flag);
void cpu_lower_interrupt(short flag);
void cpu_set_breakpoint(short addr, char set);
char cpu_get_breakpoint(short addr);

#define MEMSIZE 070000 // MAX 0100000
#define FIELD_MASK 070000
#define PAGE_MASK 07600
#define WORD_MASK 0177
#define B12_MASK 07777
#define B8_MASK 0377
#define B7_MASK 0177
#define B4_MASK 017
#define IF_MASK 07000
#define LINK_MASK 010000
#define LINK_AC_MASK 017777
#define AC_MASK 07777
#define Z_MASK 00200
#define I_MASK 00400
#define SIGN_BIT_MASK 04000
#define LINK ((ac & LINK_MASK) >> 12)
#define DEV_MASK 0770
#define IOT_OP_MASK 07
#define MMU_DI_MASK 070
#define BREAKPOINT 0100000

#define INSTR(x) ((x)<<9)
#define INC_12BIT(x) (((x)+1) & B12_MASK)
#define INC_PC(x) (((x) & FIELD_MASK) | INC_12BIT((x)));
#define INTR(x) (1 << (x))

#define TTYO_INTR_FLAG INTR(0)
#define TTYI_INTR_FLAG INTR(1)
#define UINTR_FLAG INTR(2)
#define RX_INTR_FLAG INTR(3)

#define AND INSTR(0)
#define TAD INSTR(1)
#define ISZ INSTR(2)
#define DCA INSTR(3)
#define JMS INSTR(4)
#define JMP INSTR(5)
#define IOT INSTR(6)
#define OPR INSTR(7)

#define OPR_G2    0400
#define OPR_AND   010
#define OPR_G3    01

#define CLA 0200
#define CLL 0100
#define CMA 0040
#define CML 0020
#define RAR 0010
#define RAL 0004
#define BSW 0002
#define IAC 0001

#define SMA 0100
#define SZA 0040
#define SNL 0020

#define SPA 0100
#define SNA 0040
#define SZL 0020

#define OSR 0004
#define HLT 0002

#define MQA 0100
#define MQL 0020

#define SKON 00
#define ION 01
#define IOF 02
#define SRQ 03
#define GTF 04
#define RTF 05
#define SGT 06
#define CAF 07

// TTY OP-codes:
#define KCF 0
#define KSF 1
#define KCC 2
#define KRS 4
#define KIE 5
#define KRB 6

#define TFL 0
#define TSF 1
#define TCF 2
#define TPC 4
#define TSK 5
#define TLS 6

// KM8E OP-codes:
#define CDF 01
#define CIF 02
#define CDI 03
#define CINT 000
#define RDF 010
#define RIF 020
#define RIB 030
#define RMF 040
#define SINT 050
#define CUF 060
#define SUF 070

typedef enum register_name {
  AC,
  PC,
  MQ,
  DF,
  IF,
  IB,
  UB,
  UF,
  SF,
  SR,
  ION_FLAG,
  ION_DELAY,
  INTR_INHIBIT,
  INTR,
  RTF_DELAY,
  TTY_KB_BUF,
  TTY_KB_FLAG,
  TTY_TP_BUF,
  TTY_TP_FLAG,
  TTY_DCR,
  RX_IR,
  RX_TR,
  RX_DF,
  RX_EF,
  RX_ONLINE,
  RX_BIT_MODE,
  RX_MAINTENANCE_MODE,
  RX_INTR_ENABLED,
  RX_RUN,
  RX_FUNCTION,
  RX_READY_0,
  RX_READY_1
} register_name_t;

#endif // _CPU_H_
