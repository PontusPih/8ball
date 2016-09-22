#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include "linenoise.h"

#define LINENOISE_MAX_LINE 4096

#define MEMSIZE 0100000
#define FIELD_MASK 070000
#define DF_MASK 00003
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

// TODO initialize memory properly!
short mem[MEMSIZE];

// TODO implement "clear" command that initializes these variables,
// just like the clear switch on a real front panel.
// CPU registers
short pc = 0200;
short ib = 0; // Instruction buffer
short sf = 0; // save field
short df = 0;
short ac = 0;
short mq;
short sr = 07777;
short ion = 0; // Interrupt enable flipflop
short ion_delay = 0; //ion will be set after next fetch
short rtf_delay = 0; //ion will be set after next fetch
short intr = 0; // Interrupt requested flag
short intr_inhibit = 0; // MMU inhibit flag
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
#define RDF 010
#define RIF 020
#define RIB 030
#define RMF 040

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
    addr = mem[addr];
  }
  return addr;
}

int main (int argc, char **argv)
{
#include "rimloader.h"
  pc = 07756;

  parse_options(argc, argv);

  atexit(exit_cleanup); // register after parse_option so prev.core
                        // is not overwritten with blank state.

  // Setup console
  /* Set the completion callback. This will be called every time the
   * user uses the <tab> key. */
  linenoiseSetCompletionCallback(completion_cb);
  
  /* Load history from file. The history file is just a plain text file
   * where entries are separated by newlines. */
  /* linenoiseHistoryLoad("history.txt"); / * Load the history at startup */
  
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

  while(1){

    // TTY and console handling:
    char input;
    char nchar = read(0, &input, 1); // TODO only read() if tty_kb_flag==0
    if( nchar || in_console || tty_read_from_file ){
      if( input == 033 || in_console ){
        in_console = console();
      }

      if( (nchar || tty_read_from_file) && !tty_kb_flag ){
        // If keyboard flag is not set, try to read one char.
        if( tty_read_from_file ){
          int byte = fgetc( tty_fh );
          // printf( "read %d from file\n", byte);
          if( byte != EOF ){
            tty_kb_buf = byte;
            tty_kb_flag = 1;
          } else {
            printf("Reached end of TTY file, dropping to console. "
                   "Further reads will be from keyboard\n");
            fclose( tty_fh );
            tty_read_from_file = 0;
            in_console = console();
          }
        } else {
          tty_kb_buf = input;
          tty_kb_flag = 1;
          if( tty_dcr & TTY_IE_MASK ){
            intr = 1;
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

    // TODO, proper DF-handling
    short cur = *(mem+pc); // Much like MB register
    short addr = operand_addr(pc, 0); // Much like CPMA register
    // TODO breakpoints and watch using leftover bits

    if( (cur & I_MASK) && (cur & IF_MASK) < JMS ){
      // For indirect AND, TAD, ISZ and DCA the field is set by DF.
      // For JMP and JMS it is already set by IF.
      // For IOT and OPR it doesn't matter.
      addr = (addr & B12_MASK) | (df << 12);
    }

    if( ion && intr && (! intr_inhibit) ){
      // An interrupt occured, disable interrupts, force JMS to 0000
      cur = JMS;
      addr = 0;
      ion = 0;
      // Save KM8E registers
      // TODO save U when implemented
      sf = (pc & FIELD_MASK) >> 9 | df;
      pc = pc & B12_MASK;
      df = 0;
    } else {
      // Don't increment PC in case of an interrupt. An interrupt
      // actually occurs at the end of an execution cycle, before
      // the next fetch cycle.
      pc = INC_PC(pc); // PC is incremented after fetch, so JMP works :)
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
        // restore IF
        pc = (ib << 12) | (pc & B12_MASK);
        ib = intr_inhibit = 0;
      }
      // Jump and store return address.
      mem[addr] = (pc & B12_MASK);
      pc = (pc & FIELD_MASK) | ((addr + 1) & B12_MASK);
      break;
    case JMP:
      if( intr_inhibit ){
        // restore IF
        addr = (ib << 12) | (addr & B12_MASK);
        ib = intr_inhibit = 0;
      }
      // Unconditional Jump
      pc = addr;
      break;
    case IOT:
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
          // TODO add more fields as support is added. (GT, II, and U)
          ac = (ac & LINK_MASK) | // preserve LINK
               (LINK << 11) | (intr << 9) | (ion << 7) | sf;
          break;
        case RTF:
          rtf_delay = 1;
          // RTF allways sets ION irregardles of the ION bit in AC.
          ac = ((ac << 1) & LINK_MASK) | (ac & AC_MASK); //restore LINK bit.
          pc = ((ac << 9) & FIELD_MASK) | (pc & (PAGE_MASK | WORD_MASK)); //restore IF
          df = ac & DF_MASK;
          // TODO restore more fields. (GT, II, and U);
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
            intr = 1;
          }
          break;
        case TSF:
          if( tty_tp_flag ){
            pc = INC_PC(pc);
          }
          break;
        case TCF:
          tty_tp_flag = 0;
          intr = 0; // TODO device specific intr
          break;
        case TPC:
          tty_tp_buf = (ac & B7_MASK); // emulate ASR with 7M1
          write(1, &tty_tp_buf, 1);
          // TODO nonblocking output?
          tty_tp_flag = 1;
          if( tty_dcr & TTY_IE_MASK ){
            intr = 1;
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
            intr = 1;
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

            if( iot & CIF ){ // Found with 03
              ib = field;
              intr_inhibit = 1;
            }
          }
          break;
        case 04: // READ instruction group
          switch( cur & MMU_DI_MASK ){
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
            // TODO restore U when implemented
            ib = (sf & 070) >> 3;
            df = (sf & 07);
            intr_inhibit = 1;
            break;
          default:
            printf("IOT unsupported memory management instruction(04): NOP\n");
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
          // TODO Privileged Group Two instructions
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
          // TODO add more fields as support is added. (GT, II, and U)
          printf(" GTF (LINK = %o INTR = %o ION = %o IF = %o DF = %o)",
                 LINK, intr, ion, ((sf & 070) >> 3), sf & 07);
          break;
        case RTF:
          // TODO restore more fields. (GT, II, and U);
          printf(" RTF (LINK = %o ION = %o IF = %o DF = %o)",
                 (ac >> 11) & 1, (ac >> 7) & 1, (ac >> 3) & 0b111, ac & DF_MASK);
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
          printf(" TFL");
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
          printf(" TSK");
          break;
        case TLS:
          printf(" TLS");
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
            printf(" BTR");
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
  printf("PC = %o AC = %o MQ = %o DF = %o IB = %o SR = %o ION = %o INHIB = %o", pc, ac, mq, df, ib, sr, ion, intr_inhibit);
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
  while(!done && (line = linenoise(">> ")) != NULL) {
    if (line[0] != '\0' ) {
      linenoiseHistoryAdd(line);
      
      // TODO ignore insignificant whitespace.
      char *skip_line = line;
      while( *skip_line == ' ' ){
        skip_line++;
      }
      
      if( ! strncasecmp(skip_line, "s\0", 2) ){
        print_regs();
        printf("\t\t");
        print_instruction(pc);
        in_console = 1;
        done = 1;
      }

      if( ! strncasecmp(skip_line, "r\0", 2) ){
        in_console = 0;
        done = 1;
      }
      
      if( ! strncasecmp(skip_line, "show\0", 4) ){
        print_regs();
        printf("\n");
      }

      if( ! strncasecmp(skip_line, "d ", 2) ){
        // deposit
        skip_line += 2;
        int addr = read_15bit_octal(skip_line);
        while( *skip_line != ' ' ){
          skip_line++;
        }

        int val = read_12bit_octal(skip_line);
        mem[addr] = val;
      }

      if( ! strncasecmp(skip_line, "e ", 2) ){
        skip_line += 2;
        int start = read_15bit_octal(skip_line);
        while( *skip_line != ' ' ){
          skip_line++;
        }

        int end = read_15bit_octal(skip_line);

        while( start <= end ){
          print_instruction(start);
          start++;
        }
      }

      if( ! strncasecmp(skip_line, "set ", 4) ){
        skip_line += 4;
        if( ! strncasecmp(skip_line, "pc=", 3) ){
          skip_line += 3;
          pc = read_12bit_octal(skip_line);
        }

        if( ! strncasecmp(skip_line, "sr=", 3) ){
          skip_line += 3;
          sr = read_12bit_octal(skip_line);
        }

        if( ! strncasecmp(skip_line, "ac=", 3) ){
          skip_line += 3;
          ac = read_12bit_octal(skip_line);
        }

        if( ! strncasecmp(skip_line, "df=", 3) ){
          skip_line += 3;
          ac = read_12bit_octal(skip_line);
        }

        if( ! strncasecmp(skip_line, "tty_file=", 9) ){
          if( tty_read_from_file ){
            printf("Unable to set new file name, tty_file currently open\n");
          } else {
            skip_line += 9;
            strncpy(tty_file, skip_line, 100);
          }
        }

        if( ! strncasecmp(skip_line, "tty_src", 7) ){
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
      }
        
      if( ! strncasecmp(skip_line, "save=", 5) ){
        skip_line += 5;
        if(strnlen(skip_line, LINENOISE_MAX_LINE-5) > 0){
          int do_save=0;
          if(access(skip_line,F_OK) != -1){
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
          if(do_save && save_state(skip_line)){
            printf("CPU state saved\n");
          }
        }
      }

      if( ! strncasecmp(skip_line, "restore=", 8) ){
        skip_line += 8;
        if(strnlen(skip_line, LINENOISE_MAX_LINE-5) > 0){
          if( ! restore_state(skip_line) ) {
            printf("Unable to restore state, state unchanged\n");
          }
        }
      }

      if( ! strncasecmp(skip_line, "exit", 4) ){
        exit(EXIT_SUCCESS);
      }
      
    } 
    free(line);
  }
  
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
