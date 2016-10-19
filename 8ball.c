#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include "linenoise.h"

#define MEMSIZE 0100000
#define FIELD_MASK 070000
#define PAGE_MASK 07600
#define WORD_MASK 0177
#define B12_MASK 07777
#define B8_MASK 0377
#define B7_MASK 0177
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
// TODO use INC_12BIT in more places
#define INC_12BIT(x) (((x)+1) & B12_MASK)
#define INC_PC(x) (((x) & FIELD_MASK) | INC_12BIT((x)));

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

short mem[MEMSIZE];
short breakpoints[MEMSIZE];
char trace_instruction = 0;

// TODO implement "clear" command that initializes these variables,
// just like the clear switch on a real front panel.
// CPU registers
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
short uf = 0; // User Field
short ub = 0; // User Buffer
// TODO remove rtf_delay
short rtf_delay = 0; //ion will be set after next fetch
// TODO add F D E state bits

// TTY registers
short tty_kb_buf = 0;
short tty_kb_flag = 0;
short tty_tp_buf = 0;
short tty_tp_flag = 0;
short tty_dcr = 0; // device control register

#define TTY_SE_MASK 02
#define TTY_IE_MASK 01

void tty_reset(){
  tty_kb_buf = 0;
  tty_kb_flag = 0;
  tty_tp_buf = 0;
  tty_tp_flag = 0;
  tty_dcr = 0;
}

char tty_file[100] = "binloader.rim";
char tty_read_from_file = 0;
FILE *tty_fh = NULL;

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

#define INTR(x) (1 << (x))
#define TTY_INTR_FLAG INTR(0)
#define UINTR_FLAG INTR(1)

char in_console = 1;

void signal_handler(int signo)
{
  if(signo == SIGINT) {
    // Catch Ctrl+c and drop to console.
    printf("SIGINT caught, dropping to console.\n");
    in_console = 1;
  }
}

// termios globals
struct termios told, tnew;

void completion_cb(const char *buf, linenoiseCompletions *lc);
char console();
void print_regs();
void print_instruction(short pc);
int save_state(char *filename);
int restore_state(char *filename);
void exit_cleanup();
void parse_options(int argc, char **argv);

// flags set by options:
char exit_on_HLT = 0; // A HLT will exit the proccess with EXIT_FAILURE
char stop_after = 0; // When iterations_to_exit reaches 0, exit if run_to_stop
int stop_at = -1;
int iterations_to_exit = 0;

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
      mem[addr] = (mem[addr]+1) & B12_MASK;
    }
    addr = (addr & FIELD_MASK) | (mem[addr] & B12_MASK);
  }
  return addr;
}

int main (int argc, char **argv)
{
  memset(mem, 0, sizeof(mem));
  memset(breakpoints, 0, sizeof(breakpoints));
#include "rimloader.h"
  pc = 07756;

  parse_options(argc, argv);

  atexit(exit_cleanup); // register after parse_option so prev.core
                        // is not overwritten with blank state.

  // Setup console
  /* Set the completion callback. This will be called every time the
   * user uses the <tab> key. */
  linenoiseSetCompletionCallback(completion_cb);

  /* Set max lines of history to something arbitrary*/
  linenoiseHistorySetMaxLen(500);
  
  /* Load history from file. The history file is just a plain text file
   * where entries are separated by newlines. */
  linenoiseHistoryLoad("history.txt"); /* Load the history at startup */
  
  // Setup terminal for noncanonical mode:
  
  tcgetattr(0,&told);
  tnew = told;
  tnew.c_lflag &= ~(ICANON|ECHO);
  tnew.c_cc[VMIN] = 0; // Allow read() to return without data.
  tnew.c_cc[VTIME] = 0; // And block for 0 tenths of a second.
  tcsetattr(0, TCSANOW, &tnew);

  // Setup signal handler.
  if( signal(SIGINT, signal_handler) ){
    printf("Unable to setup signal handler\n");
    exit(EXIT_FAILURE);
  }

  int tty_skip_count = 0;
  while(1){

    // TTY and console handling:
    if( !tty_kb_flag && (tty_skip_count++ > 100)){ // TODO simulate slow TTY (update maindec-d0cc to do all loops)
      tty_skip_count = 0;
      // If keyboard flag is not set, try to read one char.
      if( tty_read_from_file ){
        int byte = fgetc( tty_fh );
        if( byte != EOF ){
          tty_kb_buf = byte;
          tty_kb_flag = 1;
        } else {
          printf("Reached end of TTY file, dropping to console. "
                 "Further reads will be from keyboard\n");
          fclose( tty_fh );
          tty_read_from_file = 0;
          in_console = 1;
        }
      } else {
        char input;
        if( read(0, &input, 1) > 0 ){
          tty_kb_buf = input;
          tty_kb_flag = 1;
          if( tty_dcr & TTY_IE_MASK ){
            intr |= TTY_INTR_FLAG;
          }
        }
      }
    }

    if( stop_after && (iterations_to_exit-- == 0) ){
      printf("\n >>> STOP AFTER <<<\n");
      print_regs();
      printf("\n");
      exit(EXIT_SUCCESS);
    }

    if( stop_at > 0 && pc == stop_at ){
      printf("\n >>> STOP AT <<<\n");
      print_regs();
      printf("\n");
      exit(EXIT_SUCCESS);
    }

    if( breakpoints[pc] & BREAKPOINT ){
      printf(" >>> BREAKPOINT HIT at %o <<<\n", pc);
      // TODO add "clear all" breakpoints
      // TODO add "list" breakpoints
      in_console = 1;
    }

    if( in_console ){
      in_console = console();
    }

    short cur = *(mem+pc); // Much like MB register
    short addr = -1; // Much like CPMA register
    if( (cur & IF_MASK) <= JMP ){
      // Only calculate addr if the current instruction is an Memory
      // Referencing Instruction (MRI). Otherwise an instruction that
      // happens address an autoindexing location _and_ have the
      // indirect bit set (e.g. 6410) will ruin that location.
      addr = operand_addr(pc, 0);
    }
    // TODO add watch on memory cells

    if( (cur & I_MASK) && (cur & IF_MASK) < JMS ){
      // For indirect AND, TAD, ISZ and DCA the field is set by DF.
      // For JMP and JMS it is already set by IF.
      // For IOT and OPR it doesn't matter.
      addr = (addr & B12_MASK) | (df << 12);
    }

    if( ion && intr && (! intr_inhibit) ){
      // An interrupt occured, disable interrupts, force JMS to 0000
      if( trace_instruction ){
        printf("%.6o  %.6o INTERRUPT ==> JMS to 0", pc, cur);
      }
      cur = JMS;
      addr = 0;
      ion = 0;
      // Save KM8E registers
      sf = ((intr & UINTR_FLAG ? 1 : 0) << 6) | (pc & FIELD_MASK) >> 9 | df;
      pc = pc & B12_MASK;
      df = ib = 0;
      uf = ub = 0;
    } else {
      if( trace_instruction ){
        print_instruction(pc);
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

    switch( cur & IF_MASK ){
    case AND:
      // AND AC and operand, preserve LINK.
      ac &= (mem[addr] | LINK_MASK);
      break;
    case TAD:
      // Two complements add of AC and operand.
      // TODO: Sign extension
      ac = (ac + mem[addr]) & LINK_AC_MASK;
      break;
    case ISZ:
      // Skip next instruction if operand is zero.
      mem[addr] = INC_12BIT(mem[addr]);
      if( mem[addr] == 0 ){
        pc = INC_PC(pc);
      }
      break;
    case DCA:
      // Deposit and Clear AC
      mem[addr] = (ac & AC_MASK);
      ac = (ac & LINK_MASK);
      break;
    case JMS:
      if( intr_inhibit ){
        // restore IF and UF
        pc = (ib << 12) | (pc & B12_MASK);
        addr = (ib << 12) | (addr & B12_MASK);
        uf = ub;
        intr_inhibit = 0;
      }
      // Jump and store return address.
      mem[addr] = (pc & B12_MASK);
      pc = (pc & FIELD_MASK) | ((addr + 1) & B12_MASK);
      break;
    case JMP:
      if( intr_inhibit ){
        // restore IF and UF
        addr = (ib << 12) | (addr & B12_MASK);
        uf = ub;
        intr_inhibit = 0;
      }
      // Unconditional Jump
      pc = addr;
      break;
    case IOT:
      if( uf ){ // IOT is a privileged instruction, interrupt
        intr |= UINTR_FLAG;
        break;
      }
      switch( (cur & DEV_MASK) >> 3 ){
      case 00: // Interrupt control
        switch( cur & IOT_OP_MASK ){
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
          // TODO add more fields as support is added. (GT)
          ac = (ac & LINK_MASK) | // preserve LINK
               (LINK << 11) | ((intr & UINTR_FLAG ? 1:0) << 6) | ((intr ? 1:0) << 9) | (ion << 7) | sf;
          break;
        case RTF:
          // RTF allways sets ION irregardles of the ION bit in AC.
          ion = 1;
          intr_inhibit = 1;
          ac = ((ac << 1) & LINK_MASK) | (ac & AC_MASK); //restore LINK bit.
          ib = (ac & 070) >> 3;
          df = ac & 07;
          ub = (ac & 0100) >> 6;
          // TODO restore more fields. (GT, and U);
          break;
        case SGT:
          // TODO add with EAE support
          break;
        case CAF:
          // TODO reset supported devices. Reset MMU interrupt inhibit flipflop
          tty_reset();
          tty_dcr = TTY_IE_MASK;
          ac = ion = intr = 0;
          break;
        }
        break;
      // KL8E (device code 03 and 04)
      case 03: // Console tty input
        switch( cur & IOT_OP_MASK ){
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
          printf("illegal IOT instruction. device 03 - keyboard\n");
          in_console = 1;
          break;
        }
        break;
      case 04: // Console tty output
        switch( cur & IOT_OP_MASK ){
        case TFL:
          tty_tp_flag = 1;
          if( tty_dcr & TTY_IE_MASK ){
            intr |= TTY_INTR_FLAG;
          }
          break;
        case TSF:
          if( tty_tp_flag ){
            pc = INC_PC(pc);
          }
          break;
        case TCF:
          tty_tp_flag = 0;
          intr &= ~TTY_INTR_FLAG;
          break;
        case TPC:
          tty_tp_buf = (ac & B7_MASK); // emulate ASR with 7M1
          write(1, &tty_tp_buf, 1);
          // TODO nonblocking output?
          tty_tp_flag = 1;
          if( tty_dcr & TTY_IE_MASK ){
            intr |= TTY_INTR_FLAG;
          }
          break;
        case TSK:
          if( tty_tp_flag || tty_kb_flag ){
            pc = INC_PC(pc);
          }
          break;
        case TLS:
          tty_tp_flag = 0;
          tty_tp_buf = (ac & B7_MASK); // emulate ASR with 7M1
          write(1, &tty_tp_buf, 1);
          // TODO nonblocking output?
          tty_tp_flag = 1;
          if( tty_dcr & TTY_IE_MASK ){
            intr |= TTY_INTR_FLAG;
          }
          break;
        default:
          printf("illegal IOT instruction. device 04 - TTY output\n");
          in_console = 1;
          break;
        }
        break;
      case 020:
      case 021:
      case 022:
      case 023:
      case 024:
      case 025:
      case 026:
      case 027:
        switch( cur & IOT_OP_MASK ){
        case CDF:
        case CIF:
        case CDI:
          {
            short field = (cur & MMU_DI_MASK) >> 3;
            short iot = cur & IOT_OP_MASK;
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
          switch( cur & MMU_DI_MASK ){
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
            //printf("IOT unsupported memory management instruction(04): NOP\n");
            break;
          }
          break;
        default:
          printf("IOT unsupported memory management instruction: NOP\n");
          break;
        }
        break;
      default:
        printf("IOT to unknown device: %.3o. Treating as NOP\n", (cur & DEV_MASK) >> 3);
        break;
        // in_console = 1;
      }
      break;
    case OPR:
      if( ! (cur & OPR_G2) ){
        // Group one
        if( cur & CLA ){
          // CLear Accumulator
          ac &= LINK_MASK; 
        }

        if( cur & CLL ){
          // CLear Link
          ac &= AC_MASK;
        }

        if( cur & CMA ){
          // Complement AC
          ac = ac ^ AC_MASK;
        }
 
        if( cur & CML ){
          // Complement Link
          ac = ac ^ LINK_MASK;
        }

        if( cur & IAC ){
          // Increment ACcumulator and LINK
          ac = (ac+1) & LINK_AC_MASK;
        }

        if( cur & RAR ){
          // Rotate Accumulator Right
          char i = 1;
          if( cur & BSW ){
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

        if( cur & RAL ){
          // Rotate Accumulator Left
          char i = 1;
          if( cur & BSW ){
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

        if( ( cur & (RAR|RAL|BSW) ) == BSW ){
          // Byte Swap.
          short msb = (ac & 07700) >> 6;
          short lsb = (ac & 00077) << 6;
          ac = (ac & LINK_MASK) | msb | lsb;
        }

      } else {
        if( ! (cur & OPR_G3 ) ){
          // Group two
          if( ! (cur & OPR_AND) ) {
            // OR group
            char do_skip = 0;

            if( cur & SMA && ac & SIGN_BIT_MASK ){
              // Skip if Minus Accumulator
              do_skip = 1;
            }

            if( cur & SZA && ! (ac & AC_MASK) ){
              // Skip if Zero Accumulator
              do_skip = 1;
            }

            if( cur & SNL && (ac & LINK_MASK ) ){
              // Skip if Nonzero Link
              do_skip = 1;
            }
 
            if( cur & CLA ){
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

            if( cur & SPA && ac & SIGN_BIT_MASK ){
              // Skip if Positive Accumulator
              // Test for negative accumulator
              spa_skip = 0;
            }

            if( cur & SNA && !(ac & AC_MASK) ){
              // Skip if Nonzero Accumulator
              // Test for zero accumlator
              sna_skip = 0;
            }

            if( cur & SZL && ac & LINK_MASK ){
              // Skip if Zero Link
              // Test for nonzero link
              szl_skip = 0;
            }

            if( cur & CLA ){
              // CLear Accumulator
              ac = ac & LINK_MASK;
            }

            if( spa_skip && sna_skip && szl_skip ){
              pc = INC_PC(pc);
            }
          }
          if( uf && ( cur & (OSR | HLT) ) ){ // OSR & HLT is privileged instructions, interrupt
            intr |= UINTR_FLAG;
            break;
          }
          if( cur & OSR ){
            ac |= sr;
          }
          if( cur & HLT ){
            in_console = 1;
            printf(" >>> CPU HALTED <<<\n");
            print_regs();
            printf("\n");
            if( exit_on_HLT ){
              exit(EXIT_FAILURE);
            }
          }
        } else {
          // Group Three
          if( cur & CLA ){
            // CLear Accumulator
            ac = ac & LINK_MASK;
          }

          if( (cur & MQA) && (cur & MQL) ){
            // Swap ac and mq
            short tmp = mq & B12_MASK;
            mq = ac & AC_MASK;
            ac = tmp;
          } else {
            // Otherwise apply MQA or MQL separately
            if( cur & MQA ){
              ac = ac | (mq & B12_MASK);
            }

            if( cur & MQL ){
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
  }
}

void print_instruction(short pc)
{ 
  short cur = *(mem + pc);
  short addr = operand_addr(pc, 1);

  printf("%.6o  %.6o", pc, cur);

  if( (cur & IF_MASK) <= JMP ){
    switch( cur & IF_MASK ){
    case AND:
      printf(" AND");
      break;
    case TAD:
      printf(" TAD");
      break;
    case ISZ:
      printf(" ISZ");
      break;
    case DCA:
      printf(" DCA");
      break;
    case JMS:
      printf(" JMS");
      break;
    case JMP:
      printf(" JMP");
      break;
    }
    // TODO print value of memory cell referenced.
    if( ! (cur & Z_MASK) ) {
      if( cur & I_MASK ){
        printf(" I Z %.6o (%.6o)", direct_addr(pc), addr);
      } else {
        printf(" Z %.6o", addr);
      }
    } else {
      if( cur & I_MASK ){
        printf(" I %.6o (%.6o)", direct_addr(pc), addr);
      } else {
        printf(" %.6o", addr);
      }
    }
  } else {
    switch( cur & IF_MASK ){
    case IOT:
      switch( (cur & DEV_MASK) >> 3 ){
      case 00: // Interrupt control
        switch( cur & IOT_OP_MASK ){
        case SKON:
          printf(" SKON");
          break;
        case ION:
          printf(" ION");
          break;
        case IOF:
          printf(" IOF");
          break;
        case SRQ:
          printf(" SRQ");
          break;
        case GTF:
          // TODO add more fields as support is added. (GT)
          printf(" GTF (LINK = %o INTR = %o ION = %o U = %o IF = %o DF = %o)",
                 LINK, intr, ion, ((sf & 0100) >> 6), ((sf & 070) >> 3), sf & 07);
          break;
        case RTF:
          // TODO restore more fields. (GT);
          printf(" RTF (LINK = %o INHIB = %o ION = %o U = %o IF = %o DF = %o)",
                 (ac >> 11) & 1, (ac >> 8) & 1, (ac >> 7) & 1, (ac >> 6) & 1, (ac >> 3) & 07, ac & 07);
          break;
        case SGT:
          // TODO add with EAE support
          printf(" SGT");
          break;
        case CAF:
          printf(" CAF");
          break;
        }
        break;
        // KL8E (device code 03 and 04)
      case 03: // Console tty input
        switch( cur & IOT_OP_MASK ){
        case KCF:
          printf(" KCF");
          break;
        case KSF:
          printf(" KSF");
          break;
        case KCC:
          printf(" KCC");
          break;
        case KRS:
          printf(" KRS");
          break;
        case KIE:
          printf(" KIE");
          break;
        case KRB:
          printf(" KRB");
          break;
        default:
          printf("illegal IOT instruction. device 03 - keyboard\n");
          break;
        }
        break;
      case 04: // Console tty output
        switch( cur & IOT_OP_MASK ){
        case TFL:
          printf(" TFL"); // TODO called SPF in some assemblers?
          break;
        case TSF:
          printf(" TSF");
          break;
        case TCF:
          printf(" TCF");
          break;
        case TPC:
          printf(" TPC");
          break;
        case TSK:
          printf(" TSK"); // TODO called SPI in some assemblers?
          break;
        case TLS:
          printf(" TLS");
          break;
        default:
          printf("illegal IOT instruction. device 04 - TTY output\n");
          break;
        }
        break;
      case 020:
      case 021:
      case 022:
      case 023:
      case 024:
      case 025:
      case 026:
      case 027: // Memory management
        switch( cur & IOT_OP_MASK ){
        case 01:
          printf(" CDF");
          break;
        case 02:
          printf(" CIF");
          break;
        case 03:
          printf(" CDI");
          break;
        case 04:
          // Mask out the "FIELD" bits which are OPCODES here.
          switch( (cur & MMU_DI_MASK) >> 3 ){
          case 00:
            printf(" CINT");
            break;
          case 01:
            printf(" RDF");
            break;
          case 02:
            printf(" RIF");
            break;
          case 03:
            printf(" RIB");
            break;
          case 04:
            printf(" RMF");
            break;
          case 05:
            printf(" SINT");
            break;
          case 06:
            printf(" CUF");
            break;
          case 07:
            printf(" SUF");
            break;
          default:
            printf(" illegal IOT instruction. MMU(1) device");
            break;
          }
          break;
        default:
          printf(" illegal IOT instruction. MMU(2) device");
          break;
        }
        break;
      default:
        printf(" IOT Device: %.3o", (cur & DEV_MASK) >> 3);
        break;
      }
      break;
    case OPR:
      if( ! (cur & OPR_G2) ){
        if( cur & CLA ){
          printf(" CLA");
        }

        if( cur & CLL ){
          printf(" CLL");
        }

        if( cur & CMA ){
          printf(" CMA");
        }

        if( cur & CML ){
          printf(" CML");
        }

        if( cur & IAC ){
          printf(" IAC");
        }

        // TODO CMA & IAC is called CIA in some assemblers, support?

        if( cur & RAR ){
          if( cur & BSW ){
            printf(" RTR");
          } else {
            printf(" RAR");
          }
        }

        if( cur & RAL ){
          if( cur & BSW ){
            printf(" RTL");
          } else {
            printf(" RAL");
          }
        }

        if( ( cur & (RAR|RAL|BSW) ) == BSW ){
          printf(" BSW");
        }

        if( cur == OPR ){
          printf(" NOP");
        }

      } else {
        if( ! (cur & OPR_G3 ) ){
          if( ! (cur & OPR_AND) ) {
            if( cur & SMA ){
              printf(" SMA");
            }

            if( cur & SZA ){
              printf(" SZA");
            }

            if( cur & SNL ){
              printf(" SNL");
            }

            if( cur & CLA ){
              printf(" CLA");
            }
          } else {
            if( cur & SPA ){
              printf(" SPA");
            }

            if( cur & SNA ){
              printf(" SNA");
            }

            if( cur & SZL ){
              printf(" SZL");
            }

            if( cur & CLA ){
              printf(" CLA");
            }

            if( ! (cur & (SPA|SNA|SZL)) ){
              printf(" SKP");
            }
          }

          if( cur & OSR ){
            printf(" OSR");
            // TODO print more detail.
          }
          if( cur & HLT ){
            printf(" HLT");
          }
        } else {
          if( cur & CLA ){
            printf(" CLA");
          }

          if( cur & MQA ){
            printf(" MQA");
          }

          if( cur & MQL ){
            printf(" MQL");
          }

          // TODO CLA & MQL is called CAM in some assemblers, support?
          //      MQA & MQL is called SWP in some assemblers.
          //      CLA & OSR is called LAS
        }
      }
      break;
    }
  }
  printf("\n");
}

void completion_cb(__attribute__((unused)) const char *buf, __attribute__((unused)) linenoiseCompletions *lc){

}

void print_regs()
{
  printf("PC = %o AC = %o MQ = %o DF = %o IB = %o U = %o SF = %o SR = %o ION = %o INHIB = %o", pc, ac, mq, df, ib, uf, sf, sr, ion, intr_inhibit);
}

short read_12bit_octal(const char *buf)
{
  unsigned int res = 0;
  sscanf(buf, "%o", &res);
  if( res > B12_MASK ){
    printf("Octal value to large, truncated: %.5o\n", res & B12_MASK);
  }
  return (short)(res & B12_MASK);
}

short read_15bit_octal(const char *buf)
{
  unsigned int res = 0;
  sscanf(buf, "%o", &res);
  if( res > 077777 ){
    printf("Octal value to large, truncated: %.6o\n", res & 077777);
  }
  return (short)(res & 077777);
}

char console()
{
  char *line;
  char done = 0;
  char in_console = 0;
  while(!done){
    line = linenoise(">> ");
    if( line == NULL ){
      if( errno == EAGAIN ){
        // linenoise will set errno to EAGAIN if it reads ctrl+c
        linenoiseHistorySave("history.txt");
        printf("^C in console, exiting.\n");
        exit(EXIT_FAILURE);
      }
      continue;
    }
    if (line[0] != '\0' ) {
      linenoiseHistoryAdd(line);
      
      char *token = strtok(line, " \t");

      if( NULL == token ){
        continue;
      }

      // TODO make sure each successfully parsed command does a "continue".
      // TODO make sure trailing garbage is treated as an error.
      if( ! strncasecmp(token, "help\0", 5) ){
        printf("Run control commands:\n\n"
               "    (b)reak    (r)un    (s)tep    (t)race\n\n"
               "Memory control commands:\n\n"
               "    (d)eposit    (e)xamine     (sh)ow    (sa)ve    (re)store\n\n"
               "Device specific:\n\n"
               "    (tty_a)ttach   (tty_s)ource\n\n"
               "Emulator control:\n\n"
               "    (exit)\n\n"
               "Type \"help command\" for details.\n\n");
        // TODO help text for each command
      }

      if( ! strncasecmp(token, "s\0", 2) || !strncasecmp(token, "step\0", 5) ){
        print_regs();
        printf("\t\t");
        print_instruction(pc);
        in_console = 1;
        done = 1;
      }

      if( ! strncasecmp(token, "r\0", 2) || ! strncasecmp(token, "run\0", 4) ){
        in_console = 0;
        done = 1;
      }
      
      if( ! strncasecmp(token, "sh\0", 2) || ! strncasecmp(token, "show\0", 5) ){
        print_regs();
        printf("\n");
      }

      if( ! strncasecmp(token, "e\0", 2) || ! strncasecmp(token, "examine\0", 8) ){
        token = strtok(NULL, " \t");
        if( NULL == token ){
          continue;
        }
        // TODO add "cpu" instead of show
        if( ! strncasecmp(token, "pc", 2) ){
          printf("PC = %o\n", pc);
          continue;
        }
        if( ! strncasecmp(token, "ac", 2) ){
          printf("AC = %o\n", ac);
          continue;
        }
        if( ! strncasecmp(token, "mq", 2) ){
          printf("MQ = %o\n", mq);
          continue;
        }
        if( ! strncasecmp(token, "sr", 2) ){
          printf("SR = %o\n", sr);
          continue;
        }
        if( ! strncasecmp(token, "ion", 3) ){
          printf("ION = %o\n", ion);
          continue;
        }
        if( ! strncasecmp(token, "intr", 4) ){
          printf("INTR = %o\n", intr);
          continue;
        }
        if( ! strncasecmp(token, "sf", 2) ){
          printf("SF = %o\n", sf);
          continue;
        }
        if( ! strncasecmp(token, "df", 2) ){
          printf("DF = %o\n", df);
          continue;
        }
        if( ! strncasecmp(token, "if", 2) ){
          printf("IF = %o\n", (pc & IF_MASK) >> 12);
          continue;
        }
        if( ! strncasecmp(token, "inhib", 2) ){
          printf("INHIB = %o\n", intr_inhibit);
          continue;
        }
        if( ! strncasecmp(token, "ib", 2) ){
          printf("IB = %o\n", ib);
          continue;
        }
        if( ! strncasecmp(token, "uf", 2) ){
          printf("UF = %o\n", uf);
          continue;
        }
        if( ! strncasecmp(token, "ub", 2) ){
          printf("UB = %o\n", ub);
          continue;
        }
        if( ! strncasecmp(token, "tty", 3) ){
          printf("TTY keyboard: buf = %o flag = %d\n"
                 "TTY printer:  buf = %o flag = %d\n"
                 "TTY DCR = %o\n", tty_kb_buf, tty_kb_flag, tty_tp_buf, tty_tp_flag, tty_dcr);
          continue;
        }

        int start = read_15bit_octal(token);
        int end = start;

        token = strtok(NULL, " \t");
        if( NULL != token ){
          end = read_15bit_octal(token);
        }

        if( start == -1 || end == -1 ){
          continue;
        }

        while( start <= end ){
          print_instruction(start);
          start++;
        }
        continue;
      }

      if( ! strncasecmp(token, "b\0", 2) || !strncasecmp(token, "break\0", 6) ){
        token = strtok(NULL, " \t");
        int val = read_15bit_octal(token);
        if( val > 0 && val < MEMSIZE ){
          breakpoints[val] = breakpoints[val] ^ BREAKPOINT;
          if( breakpoints[val] ){
            printf("Breakpoint set at %o\n", val);
          } else {
            printf("Breakpoint at %o cleared\n", val);
          }
        }
      }

      if( ! strncasecmp(token, "t\0", 2) || ! strncasecmp(token, "trace\0", 6) ){
        // TODO printe trace status
        trace_instruction = !trace_instruction;
      }

      if( ! strncasecmp(token, "d\0", 2) || ! strncasecmp(token, "deposit\0", 8) ){
        token = strtok(NULL, " \t");
        if( NULL == token ){
          // TODO error message
          continue;
        }

        if( ! strncasecmp(token, "pc\0", 3) ){
          token = strtok(NULL, " \t");
          if( NULL == token ){
            // TODO error message
            continue;
          }
          short val = read_15bit_octal(token);
          if( val > 0 ){
            pc = val;
            printf("PC = %o\n", pc);
          }
          continue;
        }

        if( ! strncasecmp(token, "sr\0", 3) ){
          token = strtok(NULL, " \t");
          if( NULL == token ){
            // TODO error message
            continue;
          }
          short val = read_12bit_octal(token);
          if( val > 0 ){
            sr = val;
            printf("SR = %o\n", sr);
          }
          continue;
        }

        if( ! strncasecmp(token, "ac\0", 3) ){
          token = strtok(NULL, " \t");
          if( NULL == token ){
            // TODO error message
            continue;
          }
          short val = read_12bit_octal(token);
          if( val > 0 ){
            ac = val;
            printf("AC = %o\n", ac);
          }
          continue;
        }

        if( ! strncasecmp(token, "df\0", 3) ){
          token = strtok(NULL, " \t");
          if( NULL == token ){
            // TODO error message
            continue;
          }
          short val = read_12bit_octal(token);
          if( val > 0 ){
            df = val;
            printf("DF = %o\n", df);
          }
          continue;
        }

        if( ! strncasecmp(token, "mq\0", 3) ){
          token = strtok(NULL, " \t");
          if( NULL == token ){
            // TODO error message
            continue;
          }
          short val = read_12bit_octal(token);
          if( val > 0 ){
            mq = val;
            printf("MQ = %o\n", mq);
          }
          continue;
        }

        if( ! strncasecmp(token, "if\0", 3) ){
          token = strtok(NULL, " \t");
          if( NULL == token ){
            // TODO error message
            continue;
          }
          short val = read_12bit_octal(token);
          if( val >= 0 ){
            pc = ((val << 12 ) & FIELD_MASK) | (pc & B12_MASK);
            printf("IF = %o\n", (pc & FIELD_MASK) >> 12);
          }
          continue;
        }

        short addr=-1, val=-1;
        if( NULL != token ){
          addr = read_15bit_octal(token);
        }

        token = strtok(NULL, " \t");
        if( NULL != token ){
          val = read_12bit_octal(token);
        }

        if( addr >= 0 && val >= 0 ){
          mem[addr] = val;
          print_instruction(addr);
        }
        continue;
      }

      if( ! strncasecmp(token, "tty_a\0", 6) || ! strncasecmp(token, "tty_attach\0", 11) ){
        if( tty_read_from_file ){
          printf("Unable to set new file name, tty_file currently open\n");
        } else {
          token = strtok(NULL, " \t");
          if( NULL != token ){
            strncpy(tty_file, token, 100);
            continue;
          } else {
            // TODO error
          }
        }
      }

      if( ! strncasecmp(token, "tty_s\0", 6) || ! strncasecmp(token, "tty_source\0", 11) ){
        tty_read_from_file = ! tty_read_from_file;
        if( tty_read_from_file ){
          tty_fh = fopen(tty_file,"r");
          if( tty_fh == NULL ){
            tty_read_from_file = 0;
            printf("Unable to open: \"%s\". Input from keyboard\n", tty_file);
          } else {
            printf("TTY input from file: \"%s\"\n", tty_file);
          }
        } else {
          if( fclose(tty_fh) ){
            printf("Unable to close tty_file\n");
          }
          printf("TTY input from keyboard\n");
        }
      }
        
      if( ! strncasecmp(token, "sa\0", 3) || ! strncasecmp(token, "save\0", 5) ){
        token = strtok(NULL, " \t");
        if( NULL != token ){
          int do_save=0;
          if(access(token,F_OK) != -1){
            printf("File exists, are you sure? [Y/N]\n");
            char input;
            while(read(0, &input, 1)==0){};
            printf("%c\n",input);
            if(input == 'Y' || input == 'y'){
              do_save=1;
            }
          } else {
            do_save=1;
          }
          if(do_save && save_state(token)){
            printf("CPU state saved\n");
          }
        }
        continue;
      }

      if( !strncasecmp(token, "re\0", 3) || ! strncasecmp(token, "restore\0", 8) ){
        token = strtok(NULL, " \t");
        if( NULL != token ){
          if( ! restore_state(token) ) {
            printf("Unable to restore state, state unchanged\n");
          } else {
            printf("CPU state restored\n");
          }
        }
        continue;
      }

      if( ! strncasecmp(skip_line, "quit\0", 5) || ! strncasecmp(skip_line, "exit\0", 5) ){
        linenoiseHistorySave("history.txt");
        exit(EXIT_SUCCESS);
      }
      
    } 
    free(line);
  }

  linenoiseHistorySave("history.txt");
  return in_console;
}


void exit_cleanup()
{
  save_state("prev.core");
  tcsetattr(0, TCSANOW, &told);
}


int save_state(char *filename)
{
  FILE *core = fopen(filename, "w+");

  if( NULL == core ){
    perror("Unable to open state file");
    return 0;
  }

  // TODO save/restore more state variables
  fprintf(core, "8BALL MEM DUMP VERSION=1\n");
  fprintf(core, "CPU STATE:\n");
  fprintf(core, "PC = %.5o AC = %.4o MQ = %.4o DF = %.2o SR = %.4o\n",
          pc, ac, mq, df, sr);
  fprintf(core, "ION = %.2o ION_DELAY = %.2o RTF_DELAY = %.2o INTR = %.2o\n",
          ion, ion_delay, rtf_delay, intr);

  fprintf(core, "MEMORY:\n");
  int i = 0;
  for(int f=0;f < 8;f++){
    fprintf(core, "FIELD %.2o\n", f);
    for(int p=0;p < 32;p++){
      fprintf(core, "PAGE %.3o\n",p);
      for(int row=0;row <8;row++){
        for(int col=0;col<16;col++){
          fprintf(core,"%.4o ",mem[i++]);
        }
        fprintf(core, "\n");
      }
    }
  }
  if(i != MEMSIZE){
    printf("+++ INTERNAL ERROR MEM OUT OF CHEEZE +++");
    return 0;
  }

  if( fclose(core) ){
    perror("Unable to close state file");
    return 0;
  }
  return 1;
}


int restore_state(char *filename)
{
  FILE *core = fopen(filename, "r");

  if( NULL == core ){
    perror("Unable to open state file");
    return 0;
  }

  int version=-1;
  int res=-1;
  res = fscanf(core, "8BALL MEM DUMP VERSION=%d\n", &version);
  if( ! ( 1 == res && version == 1 ) ){
    printf("Unable to parse version string");
    return 0;
  }

  int length = 0;
  res = fscanf(core, "CPU STATE:\n%n", &length);
  if( length != strlen("CPU STATE:\n") ){
    printf("Unable to find CPU STATE\n");
    return 0;
  }

  unsigned int rpc, rac, rmq, rdf, rsr;
  res = fscanf(core, "PC = %o AC = %o MQ = %o DF = %o SR = %o\n",
               &rpc, &rac, &rmq, &rdf, &rsr);
  if( ! (5 == res) ){
    printf("Unable to parse register set 1\n");
    return 0;
  }

  unsigned int rion, rion_delay, rrtf_delay, rintr;
  res = fscanf(core, "ION = %o ION_DELAY = %o RTF_DELAY = %o INTR = %o\n",
               &rion, &rion_delay, &rrtf_delay, &rintr);
  if( ! (4 == res) ){
    printf("Unable to parse register set 2\n");
    return 0;
  }

  res = fscanf(core, "MEMORY:\n%n", &length);
  if( length != strlen("MEMORY:\n") ){
    printf("Unable to find MEMORY\n");
    return 0;
  }

  int i = 0, field_no, page_no;
  unsigned short rmem[MEMSIZE];
  for(int f=0;f < 8;f++){
    res = fscanf(core, "FIELD %o\n", &field_no);
    if( !( 1 == res && field_no == f ) ){
      printf("Unable fo find FIELD %.2o\n",f);
      return 0;
    }
    for(int p=0;p < 32;p++){
      fprintf(core, "PAGE %.3o\n",p);
      res = fscanf(core, "PAGE %o\n", &page_no);
      if( !( 1 == res && page_no == p ) ){
        printf("Unable to find PAGE %.3o\n",p);
        return 0;
      }
      for(int row=0;row <8;row++){
        for(int col=0;col<16;col++){
          fscanf(core,"%ho ",&rmem[i++]);
        }
      }
    }
  }
  if(i != MEMSIZE){
    printf("Unable to find all memory\n");
    return 0;
  }

  memcpy(mem, rmem, sizeof(mem));
  pc = rpc;
  ac = rac;
  mq = rmq;
  df = rdf;
  sr = rsr;
  ion = rion;
  ion_delay = rion_delay;
  rtf_delay = rrtf_delay;
  intr = rintr;
  return 1;
}


void parse_options(int argc, char **argv)
{
  while (1) {
    int c;
    int option_index;

    static struct option long_options[] = {
      {"restore",     required_argument, 0, 'r' },
      {"exit_on_HLT", no_argument,       0, 'e' },
      {"stop_after",  required_argument, 0, 's' },
      {"stop_at",     required_argument, 0, 't' },
      {"pc",          required_argument, 0, 'p' },
      {"run",         no_argument,       0, 'n' },
      {0,             0,                 0, 0 }
    };

    c = getopt_long(argc, argv, "",long_options, &option_index);

    if ( c == -1 ){
      break;
    }

    switch (c) {
    case 'r':
      if( ! restore_state(optarg) ){
        exit(EXIT_FAILURE);
      }
      break;

    case 'e':
      exit_on_HLT = 1;
      break;

    case 's':
      stop_after = 1;
      iterations_to_exit = atoi(optarg); // TODO strtol
      break;

    case 't':
      stop_at = atoi(optarg);
      break;

    case 'p':
      pc = atoi(optarg); // TODO octal format
      break;

    case 'n':
      in_console = 0;
      break;

    case '?':
      exit(EXIT_FAILURE);
      break;

    default:
      printf("?? getopt returned character code 0%o ??\n", c);
      break;
    }
  }
}
