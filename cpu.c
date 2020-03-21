/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#include "cpu.h"
#include "tty.h"

// TODO implement "clear" command that initializes these variables,
// just like the clear switch on a real front panel.
// CPU registers
short mb = 0; // Memory Buffer
short cpma = 0; // Central Processor Memory Address
short pc = 0200; // Program Counter (and Instruction Field, if)
short ac = 0; // Acumulator
short mq = 0; // Multiplier Quotient
short sr = 07777; // Switch Registers, 1 is switch up
short ion = 0; // Interrupt enable flipflop
short ion_delay = 0; //ion will be set after next fetch
short intr = 0; // Interrupt requested flags
// Memory extension registers
short ib = 0; // Instruction buffer
short sf = 0; // save field
short df = 0; // data field
short intr_inhibit = 0; // interrupt inhibit flag
// Time share registers
short uf = 0; // User Flag
short ub = 0; // User Buffer
// TODO remove rtf_delay
short rtf_delay = 0; //ion will be set after next fetch
// TODO add F D E state bits

short mem[MEMSIZE];
short breakpoints[MEMSIZE];

void cpu_init(void){
  int i;
  for( i=0 ; i<MEMSIZE; i++){
    mem[i] = 0;
    breakpoints[i] = 0;
  }
#include "rimloader.h"
  pc = 07756;
}


short direct_addr(short pc)
{
  short cur = *(mem+pc);
  short addr = 0;

  if( cur & Z_MASK ){
    // page addressing
    addr = pc & (FIELD_MASK|PAGE_MASK);
  } else {
    // page zero mode
    addr = pc & (FIELD_MASK);
  }
  
  addr |= cur & WORD_MASK;

  return addr;
}

// Memory access modifies some magic addresses. If "examine" is
// non-zero, no modification is done.
short operand_addr(short pc, char examine)
{
  short cur = *(mem+pc);
  short addr = direct_addr(pc);

  if( cur & I_MASK ){
    // indirect addressing
    if( (addr & (PAGE_MASK|WORD_MASK)) >= 010 
        &&
        (addr & (PAGE_MASK|WORD_MASK)) <= 017
        &&
        ! examine ){
      // autoindex addressing
      mem[addr] = INC_12BIT(mem[addr]);
    }
    addr = (addr & FIELD_MASK) | (mem[addr] & B12_MASK);
  }
  return addr;
}


void cpu_raise_interrupt(short flag)
{
  intr |= flag;
}


int cpu_process()
{
  if( ion && intr && (! intr_inhibit) ){
    // An interrupt occured, disable interrupts, force JMS to 0000
    mb = JMS;
    cpma = 0;
    ion = 0;
    // Save KM8E registers
    sf = (uf << 6) | (pc & FIELD_MASK) >> 9 | df;
    pc = pc & B12_MASK; // Clear the field bits
    df = ib = 0;
    uf = ub = 0;
  } else {
    // No interrupt, enter TS1 of FETCH major state
    mb = *(mem+pc);

    if( (mb & IF_MASK) <= JMP ){
      // Only calculate cpma if the current instruction is a Memory
      // Referencing Instruction (MRI). Otherwise an instruction that
      // happens to address an autoindexing location _and_ have the
      // indirect bit set (e.g. 6410) will ruin that location.
      cpma = operand_addr(pc, 0);
    }
    // TODO add watch on memory cells

    if( (mb & I_MASK) && (mb & IF_MASK) < JMS ){
      // For indirect AND, TAD, ISZ and DCA the field is set by DF.
      // For JMP and JMS it is already set by IF.
      // For IOT and OPR it doesn't matter.
      cpma = (cpma & B12_MASK) | (df << 12);
    }

    // Don't increment PC in case of an interrupt. An interrupt
    // actually occurs at the end of an execution cycle, before
    // the next fetch cycle.
    pc = INC_PC(pc); // PC is incremented after fetch, so JMS works :)
  }

  if( ion_delay ){
    // BUG? TODO What if an interrupt occurs between two ION instructions?
    // ION is not set until the following instruction has been
    // fetched.
    ion = 1;
    ion_delay=0;
  }

  if( rtf_delay ){
    // RTF has been executed and ION is not restored until the
    // following instruction has been fetched.
    ion = 1; // RTF always sets ION.
    rtf_delay=0;
  }


  switch( mb & IF_MASK ){
  case AND:
    // AND AC and operand, preserve LINK.
    ac &= (mem[cpma] | LINK_MASK);
    break;
  case TAD:
    // Two complements add of AC and operand.
    // TODO: Sign extension
    ac = (ac + mem[cpma]) & LINK_AC_MASK;
    break;
  case ISZ:
    // Skip next instruction if operand is zero.
    mem[cpma] = INC_12BIT(mem[cpma]);
    if( mem[cpma] == 0 ){
      pc = INC_PC(pc);
    }
    break;
  case DCA:
    // Deposit and Clear AC
    mem[cpma] = (ac & AC_MASK);
    ac = (ac & LINK_MASK);
    break;
  case JMS:
    if( intr_inhibit ){
      // restore IF and UF
      pc = (ib << 12) | (pc & B12_MASK);
      cpma = (ib << 12) | (cpma & B12_MASK);
      uf = ub;
      intr_inhibit = 0;
    }
    // Jump and store return address.
    mem[cpma] = (pc & B12_MASK);
    pc = (pc & FIELD_MASK) | INC_12BIT(cpma);
    break;
  case JMP:
    if( intr_inhibit ){
      // restore IF and UF
      cpma = (ib << 12) | (cpma & B12_MASK);
      uf = ub;
      intr_inhibit = 0;
    }
    // Unconditional Jump
    pc = cpma;
    break;
  case IOT:
    if( uf ){ // IOT is a privileged instruction, interrupt
      intr |= UINTR_FLAG;
      break;
    }
    switch( (mb & DEV_MASK) >> 3 ){
    case 00: // Interrupt control
      switch( mb & IOT_OP_MASK ){
      case SKON:
        if( ion ){
          pc = INC_PC(pc);
          ion = 0;
        }
        break;
      case ION:
        ion_delay = 1;
        break;
      case IOF:
        ion = 0;
        break;
      case SRQ:
        if( intr ){
          pc = INC_PC(pc);
        }
        break;
      case GTF:
        // TODO add more fields as support is added. (GT to AC1)
        // Small Computer Handbook from -73 says intr_inhibit is saved
        // by GTF. But MAINDEC-8E-D1HA explicitly tests the opposite.
        ac = (ac & LINK_MASK) | // preserve LINK
          (LINK << 11) | ((intr ? 1:0) << 9) | (ion << 7) | sf; // Remember that UF stored in SF on an 8/E
        break;
      case RTF:
        // RTF allways sets ION irregardless of the ION bit in AC.
        ion = 1;
        intr_inhibit = 1;
        ac = ((ac << 1) & LINK_MASK) | (ac & AC_MASK); //restore LINK bit.
        ib = (ac & 070) >> 3;
        df = ac & 07;
        ub = (ac & 0100) >> 6;
        // TODO restore more fields. (GT);
        break;
      case SGT:
        // TODO add with EAE support
        break;
      case CAF:
        // TODO reset supported devices. Reset MMU interrupt inhibit flipflop
        tty_reset();
        ac = ion = intr = 0;
        break;
      }
      break;
      // KL8E (device code 03 and 04)
    case 03: // Console tty input
      switch( mb & IOT_OP_MASK ){
      case KCF:
        tty_kb_flag = 0;
        intr = intr & ~TTYI_INTR_FLAG;
        break;
      case KSF:
        if( tty_kb_flag ){
          pc = INC_PC(pc);
        }
        break;
      case KCC:
        tty_kb_flag = 0;
        intr = intr & ~TTYI_INTR_FLAG;
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
        intr = intr & ~TTYI_INTR_FLAG;
        ac &= LINK_MASK;
        ac |= (tty_kb_buf & B8_MASK);
        break;
      default:
        // TODO optionally go to console
        //printf("illegal IOT instruction. device 03 - keyboard\n");
        break;
      }
      break;
    case 04: // Console tty output
      switch( mb & IOT_OP_MASK ){
      case TFL:
        tty_tp_flag = 1;
        if( tty_dcr & TTY_IE_MASK ){
          intr |= TTYO_INTR_FLAG;
        }
        break;
      case TSF:
        if( tty_tp_flag ){
          pc = INC_PC(pc);
        }
        break;
      case TCF:
        tty_tp_flag = 0;
        intr &= ~TTYO_INTR_FLAG;
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
        intr &= ~TTYO_INTR_FLAG;
        break;
      default:
        // TODO optionally go to console
        //printf("illegal IOT instruction. device 04 - TTY output\n");
        break;
      }
      break;
    case 020: // Memory Management instructions, the last three bits
    case 021: // is the memory field being read or set.
    case 022:
    case 023:
    case 024:
    case 025:
    case 026:
    case 027:
      switch( mb & IOT_OP_MASK ){
      case CDF: // Change field instructions
      case CIF:
      case CDI:
        {
          short field = (mb & MMU_DI_MASK) >> 3;
          short iot = mb & IOT_OP_MASK;
          if( iot & CDF ){
            df = field;
          }

          if( iot & CIF ){
            ib = field;
            intr_inhibit = 1;
          }
        }
        break;
      case 04: // READ instruction group
        switch( mb & MMU_DI_MASK ){
        case CINT:
          intr &= ~UINTR_FLAG;
          break;
        case RDF:
          ac |= (df << 3);
          break;
        case RIF:
          ac |= (pc & FIELD_MASK) >> 9;
          break;
        case RIB:
          ac |= sf;
          break;
        case RMF:
          ib = (sf & 070) >> 3;
          df = (sf & 07);
          ub = (sf & 0100) >> 6;
          intr_inhibit = 1;
          break;
        case SINT:
          if( intr & UINTR_FLAG ){
            pc = INC_PC(pc);
          }
          break;
        case CUF:
          ub = 0;
          intr_inhibit = 1;
          break;
        case SUF:
          ub = 1;
          intr_inhibit = 1;
          break;
        default:
          // TODO optionally go to console
          //printf("IOT unsupported memory management instruction(04): NOP\n");
          break;
        }
        break;
      default:
        // TODO optionally go to console
        //printf("IOT unsupported memory management instruction: NOP\n");
        break;
      }
      break;
    default:
      // TODO optionally go to console
      //printf("IOT to unknown device: %.3o. Treating as NOP\n", (mb & DEV_MASK) >> 3);
      // DEV 01 High speed paper tape reader
      // DEV 02 High speed paper tape punch
      // DEV 10
      // 01 and 04 -> MP8, memory parity
      // 02 -> KP8, pwr fail & restart
      break;
    }
    break;
  case OPR:
    if( ! (mb & OPR_G2) ){
      // Group one
      if( mb & CLA ){
        // CLear Accumulator
        ac &= LINK_MASK; 
      }

      if( mb & CLL ){
        // CLear Link
        ac &= AC_MASK;
      }

      if( mb & CMA ){
        // Complement AC
        ac = ac ^ AC_MASK;
      }
 
      if( mb & CML ){
        // Complement Link
        ac = ac ^ LINK_MASK;
      }

      if( mb & IAC ){
        // Increment ACcumulator and LINK
        ac = (ac+1) & LINK_AC_MASK;
      }

      if( mb & RAR ){
        // Rotate Accumulator Right
        char i = 1;
        if( mb & BSW ){
          // Rotate Twice Right (RTR)
          i = 2;
        }
        for( ; i > 0; i-- ){
          char lsb = ac & 0b1;
          ac = ac >> 1;
          if( lsb ){
            ac |= LINK_MASK;
          } else {
            ac &= ~LINK_MASK;
          }
        }
      }

      if( mb & RAL ){
        // Rotate Accumulator Left
        char i = 1;
        if( mb & BSW ){
          // Rotate Twice Left (RTL)
          i = 2;
        }
        for( ; i > 0; i-- ){
          short msb = ac & LINK_MASK;
          ac = (ac << 1) & LINK_AC_MASK;
          if( msb ){
            ac |= 1;
          } else {
            ac &= ~1;
          }
        }
      }

      if( ( mb & (RAR|RAL|BSW) ) == BSW ){
        // Byte Swap.
        short msb = (ac & 07700) >> 6;
        short lsb = (ac & 00077) << 6;
        ac = (ac & LINK_MASK) | msb | lsb;
      }

      // TODO if RTR|RTL then AC = link | cpma & 7600 + mb & 0177
      // TODO if RAR|RAL then AC = link | mb

    } else {
      if( ! (mb & OPR_G3 ) ){
        // Group two
        if( ! (mb & OPR_AND) ) {
          // OR group
          char do_skip = 0;

          if( mb & SMA && ac & SIGN_BIT_MASK ){
            // Skip if Minus Accumulator
            do_skip = 1;
          }

          if( mb & SZA && ! (ac & AC_MASK) ){
            // Skip if Zero Accumulator
            do_skip = 1;
          }

          if( mb & SNL && (ac & LINK_MASK ) ){
            // Skip if Nonzero Link
            do_skip = 1;
          }
 
          if( mb & CLA ){
            // CLear Accumulator
            ac = ac & LINK_MASK;
          }

          if( do_skip ){
            pc = INC_PC(pc);
          }

        } else {
          // AND group

          // Assume we will skip
          char spa_skip = 1;
          char sna_skip = 1;
          char szl_skip = 1;

          if( mb & SPA && ac & SIGN_BIT_MASK ){
            // Skip if Positive Accumulator
            // Test for negative accumulator
            spa_skip = 0;
          }

          if( mb & SNA && !(ac & AC_MASK) ){
            // Skip if Nonzero Accumulator
            // Test for zero accumlator
            sna_skip = 0;
          }

          if( mb & SZL && ac & LINK_MASK ){
            // Skip if Zero Link
            // Test for nonzero link
            szl_skip = 0;
          }

          if( mb & CLA ){
            // CLear Accumulator
            ac = ac & LINK_MASK;
          }

          if( spa_skip && sna_skip && szl_skip ){
            pc = INC_PC(pc);
          }
        }
        if( uf && ( mb & (OSR | HLT) ) ){ // OSR & HLT is privileged instructions, interrupt
          intr |= UINTR_FLAG;
          break;
        }
        if( mb & OSR ){
          ac |= sr;
        }
        if( mb & HLT ){
          return -1;
        }
      } else {
        // Group Three
        if( mb & CLA ){
          // CLear Accumulator
          ac = ac & LINK_MASK;
        }

        if( (mb & MQA) && (mb & MQL) ){
          // Swap ac and mq
          short tmp = mq & B12_MASK;
          mq = ac & AC_MASK;
          ac = (ac & LINK_MASK) | tmp;
        } else {
          // Otherwise apply MQA or MQL separately
          if( mb & MQA ){
            ac = ac | (mq & B12_MASK);
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
    break;
  }
  return 0;
}
