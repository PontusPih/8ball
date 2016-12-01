#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include "linenoise.h"
#include "console.h"
#include "cpu.h"
#include "tty.h"

char in_console = 1;
char trace_instruction = 0;
// flags set by options:
char exit_on_HLT = 0; // A HLT will exit the proccess with EXIT_FAILURE
short stop_at = -1;

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
void parse_options(int argc, char **argv);
void exit_cleanup(void);

void console_setup(int argc, char **argv)
{
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
  tnew.c_lflag &= ~(ICANON|ECHO); // NON-canonical mode and no ECHO of input
  tnew.c_iflag &= ~(ICRNL); // Don't convert CR to NL
  tnew.c_iflag |= ISTRIP; // Strip bit eight
  tnew.c_cc[VMIN] = 0; // Allow read() to return without data.
  tnew.c_cc[VTIME] = 0; // And block for 0 tenths of a second.
  tcsetattr(0, TCSANOW, &tnew);

  // Setup signal handler.
  if( signal(SIGINT, signal_handler) ){
    printf("Unable to setup signal handler\n");
    exit(EXIT_FAILURE);
  }

}

void console_stop_break()
{
  if( breakpoints[pc] & BREAKPOINT ){
    printf(" >>> BREAKPOINT HIT at %o <<<\n", pc);
    in_console = 1;
  }

  if( stop_at > 0 && pc == stop_at ){
    printf("\n >>> STOP AT <<<\n");
    print_regs();
    printf("\n");
    exit(EXIT_SUCCESS);
  }
}


void console_trace_instruction()
{
  if( trace_instruction ) {
    if( ion && intr && (! intr_inhibit) ){
      // An interrupt occured, disable interrupts, force JMS to 0000
      printf("%.6o  %.6o INTERRUPT ==> JMS to 0", pc, *(mem+pc));
    } else {
      print_instruction(pc);
    }
  }
}


void print_instruction(short pc)
{ 
  short cur = *(mem + pc);
  short addr = operand_addr(pc, 1);

  printf("%.5o  %.4o", pc, cur);

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

    if( ! (cur & Z_MASK) ) {
      printf(" Z");
    } else {
      printf("  ");
    }

    if( cur & I_MASK ){
      printf(" I %.5o (%.5o)", direct_addr(pc), addr);
    } else {
      printf("   %.5o", addr);
    }

    if( (cur & IF_MASK) < JMS ){
      printf(" [%.4o]", mem[addr]);
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

char tty_file[100] = "binloader.rim";
char tty_read_from_file = 0;
FILE *tty_fh = NULL;

char read_tty_byte(char *output)
{
  if( tty_read_from_file ){
    int byte = fgetc( tty_fh );
    if( byte == EOF ){
      printf("Reached end of TTY file, dropping to console. "
             "Further reads will be from keyboard\n");
      fclose( tty_fh );
      tty_read_from_file = 0;
      in_console = 1;
      return 0;
    } else {
      *output = byte;
      return 1;
    }
  } else {
    unsigned char input;
    if( read(0, &input, 1) > 0 ){
      // ISTRIP removes bit eight, but other terminals might not.
      *output = input & B7_MASK;
      return 1;
    } else {
      return 0;
    }
  }
}

void completion_cb(__attribute__((unused)) const char *buf, __attribute__((unused)) linenoiseCompletions *lc){

}

void print_regs()
{
  printf("PC = %o AC = %o MQ = %o DF = %o IB = %o U = %o SF = %o SR = %o ION = %o INHIB = %o", pc, ac, mq, df, ib, uf, sf, sr, ion, intr_inhibit);
}

short read_12bit_octal(const char *buf)
{
  char *endptr;
  unsigned int res = strtol(buf, &endptr, 8);
  if( *endptr != '\0' ){
    printf("Unable to parse octal value.\n");
    return -1;
  }
  if( res > B12_MASK ){
    printf("Octal value to large, truncated: %.5o\n", res & B12_MASK);
  }
  return (short)(res & B12_MASK);
}

short read_15bit_octal(const char *buf)
{
  char *endptr;
  unsigned int res = strtol(buf, &endptr, 8);
  if( *endptr != '\0' ){
    printf("Unable to parse octal value: %s\n", buf);
    return -1;
  }
  if( res > 077777 ){
    printf("Octal value to large, truncated: %.6o\n", res & 077777);
  }
  return (short)(res & 077777);
}

typedef enum {
  BAD_TOKEN = -2,
  NULL_TOKEN,
  CLEAR,
  LIST,
  BREAK,
  EXAMINE,
  EXIT,
  DEPOSIT,
  HELP,
  RUN,
  SAVE,
  RESTORE,
  STEP,
  TRACE,
  TTY_ATTACH,
  TTY_SOURCE,
  E_AC,
  E_MQ,
  E_SR,
  E_ION,
  E_INTR,
  E_SF,
  E_DF,
  E_IF,
  E_INHIB,
  E_IB,
  E_UF,
  E_UB,
  E_PC,
  E_TTY,
  E_CPU,
  OCTAL_LITERAL
} token;

token map_token(char *token)
{
  if( NULL == token )
    return NULL_TOKEN;

  if( ! strcasecmp(token, "list") || ! strcasecmp(token, "l") )
    return LIST;
  if( ! strcasecmp(token, "clear") || ! strcasecmp(token, "c") )
    return CLEAR;
  if( ! strcasecmp(token, "break") || ! strcasecmp(token, "b") )
    return BREAK;
  if( ! strcasecmp(token, "examine") || ! strcasecmp(token, "e") )
    return EXAMINE;
  if( ! strcasecmp(token, "exit") || ! strcasecmp(token, "quit") )
    return EXIT;
  if( ! strcasecmp(token, "deposit") || ! strcasecmp(token, "d") )
    return DEPOSIT;
  if( ! strcasecmp(token, "help") || ! strcasecmp(token, "h") )
    return HELP;
  if( ! strcasecmp(token, "run") || ! strcasecmp(token, "r") )
    return RUN;
  if( ! strcasecmp(token, "save") || ! strcasecmp(token, "sa") )
    return SAVE;
  if( ! strcasecmp(token, "restore") || ! strcasecmp(token, "re") )
    return RESTORE;
  if( ! strcasecmp(token, "step") || ! strcasecmp(token, "s") )
    return STEP;
  if( ! strcasecmp(token, "trace") || ! strcasecmp(token, "t") )
    return TRACE;
  if( ! strcasecmp(token, "tty_attach") || ! strcasecmp(token, "tty_a") )
    return TTY_ATTACH;
  if( ! strcasecmp(token, "tty_source") || ! strcasecmp(token, "tty_s") )
    return TTY_SOURCE;
  if( ! strcasecmp(token, "ac") )
    return E_AC;
  if( ! strcasecmp(token, "mq") )
    return E_MQ;
  if( ! strcasecmp(token, "sr") )
    return E_SR;
  if( ! strcasecmp(token, "ion") )
    return E_ION;
  if( ! strcasecmp(token, "intr") )
    return E_INTR;
  if( ! strcasecmp(token, "sf") )
    return E_SF;
  if( ! strcasecmp(token, "df") )
    return E_DF;
  if( ! strcasecmp(token, "if") )
    return E_IF;
  if( ! strcasecmp(token, "inhib") )
    return E_INHIB;
  if( ! strcasecmp(token, "ib") )
    return E_IB;
  if( ! strcasecmp(token, "uf") )
    return E_UF;
  if( ! strcasecmp(token, "ub") )
    return E_UB;
  if( ! strcasecmp(token, "pc") )
    return E_PC;
  if( ! strcasecmp(token, "tty") )
    return E_TTY;
  if( ! strcasecmp(token, "cpu") )
    return E_CPU;

  char *endptr;
  strtol(token, &endptr, 8);
  if( *endptr == '\0' )
    return OCTAL_LITERAL;

  return BAD_TOKEN; // unknown token
}

void to_many_args(){
  printf("Syntax ERROR, too many arguments\n");
}

void to_few_args(){
  printf("Syntax ERROR, too few arguments\n");
}

char console(void)
{
  if( in_console < 0 ) {
    printf(" >>> CPU HALTED <<<\n");
    print_regs();
    printf("\n");
    if( exit_on_HLT ){
      exit(EXIT_FAILURE);
    }
  }

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

      char *_1st_str = strtok(line, " \t");
      char *_2nd_str = strtok(NULL, " \t");
      char *_3rd_str = strtok(NULL, " \t");

      token _1st_tok = map_token(_1st_str);
      token _2nd_tok = map_token(_2nd_str);
      token _3rd_tok = map_token(_3rd_str);

      char *trail = strtok(NULL, " \t");

      if( NULL != trail ) {
        to_many_args();
        continue;
      }

      int val = -1;

      switch(_1st_tok){
      case BREAK:
        if( NULL_TOKEN != _3rd_tok ){
          to_many_args();
          break;
        }
        if( NULL_TOKEN == _2nd_tok ){
          to_few_args();
          break;
        }

        if( CLEAR == _2nd_tok ) {
          memset(breakpoints, 0, MEMSIZE);
          printf("Breakpoints cleared\n");
          break;
        }

        if( LIST == _2nd_tok ){
          for(int i = 0; i < MEMSIZE; i++) {
            // TODO add tests
            if( breakpoints[i] ){
              printf("Breakpoint set at %o\n", i);
            }
          }
          break;
        }

        if( OCTAL_LITERAL != _2nd_tok ){
          printf("Syntax ERROR, break argument can be 'list', 'clear' or octal value\n");
          break;
        }

        val = read_15bit_octal(_2nd_str);
        if( val > 0 && val < MEMSIZE ){
          breakpoints[val] = breakpoints[val] ^ BREAKPOINT;
          if( breakpoints[val] ){
            printf("Breakpoint set at %o\n", val);
          } else {
            printf("Breakpoint at %o cleared\n", val);
          }
        } else {
          printf("ERROR, breakpoint outside memory\n");
        }

        break;
      case EXAMINE:

        if( OCTAL_LITERAL != _2nd_tok ){
          if( NULL_TOKEN != _3rd_tok ){
            to_many_args();
            break;
          }
        }

        int start, end;
        switch(_2nd_tok) {
        case E_AC:
          printf("AC = %o\n", ac);
          break;
        case E_MQ:
          printf("MQ = %o\n", mq);
          break;
        case E_SR:
          printf("SR = %o\n", sr);
          break;
        case E_ION:
          printf("ION = %o\n", ion);
          break;
        case E_INTR:
          printf("INTR = %o\n", intr);
          break;
        case E_SF:
          printf("SF = %o\n", sf);
          break;
        case E_DF:
          printf("DF = %o\n", df);
          break;
        case E_IF:
          printf("IF = %o\n", (pc & IF_MASK) >> 12);
          break;
        case E_INHIB:
          printf("INHIB = %o\n", intr_inhibit);
          break;
        case E_IB:
          printf("IB = %o\n", ib);
          break;
        case E_UF:
          printf("UF = %o\n", uf);
          break;
        case E_UB:
          printf("UB = %o\n", ub);
          break;
        case E_PC:
          printf("PC = %o\n", pc);
          break;
        case E_TTY:
          printf("TTY keyboard: buf = %o flag = %d\n"
                 "TTY printer:  buf = %o flag = %d\n"
                 "TTY DCR = %o\n", tty_kb_buf, tty_kb_flag, tty_tp_buf, tty_tp_flag, tty_dcr);
          break;
        case E_CPU:
          print_regs();
          printf("\n");
          break;
        case OCTAL_LITERAL:
          if( _3rd_tok != NULL_TOKEN && _3rd_tok != OCTAL_LITERAL ) {
            printf("Syntax ERROR, non octal end of interval\n");
            break;
          }

          start = read_15bit_octal(_2nd_str);
          end = start;

          if( OCTAL_LITERAL == _3rd_tok ){
            end = read_15bit_octal(_3rd_str);
          }

          if( start == -1 || end == -1 ){
            printf("Syntax ERROR, bad interval\n");
            break;
          }

          while( start <= end ){
            print_instruction(start);
            start++;
          }
          break;
        case BAD_TOKEN:
          printf("Syntax ERROR, examine what?\n");
          break;
        case NULL_TOKEN:
          to_few_args();
          break;
        default:
          printf("Syntax ERROR, examine what?\n");
          break;
        }
        break;
      case EXIT:
        if( NULL_TOKEN != _2nd_tok ){
          to_many_args();
          break;
        }

        linenoiseHistorySave("history.txt");
        exit(EXIT_SUCCESS);
        break;
      case DEPOSIT:
        if( NULL_TOKEN == _2nd_tok || NULL_TOKEN == _3rd_tok ){
          to_few_args();
          break;
        }

        if( OCTAL_LITERAL != _3rd_tok ){
          printf("Syntax ERROR, non-octal value to deposit\n");
          break;
        }
        int addr=-1, val=-1;
        switch( _2nd_tok ) {
        case E_AC:
          val = read_12bit_octal(_3rd_str);
          if( val >= 0 ){
            ac = val;
            printf("AC = %o\n", ac);
          }
          break;
        case E_MQ:
          val = read_12bit_octal(_3rd_str);
          if( val >= 0 ){
            mq = val;
            printf("MQ = %o\n", mq);
          }
          break;
        case E_SR:
          val = read_12bit_octal(_3rd_str);
          if( val >= 0 ){
            sr = val;
            printf("SR = %o\n", sr);
          }
          break;
        case E_DF:
          val = read_12bit_octal(_3rd_str);
          if( val >= 0 ){
            df = val;
            printf("DF = %o\n", df);
          }
          break;
        case E_IF:
          val = read_12bit_octal(_3rd_str);
          if( val >= 0 && val <= 3){
            pc = ((val << 12 ) & FIELD_MASK) | (pc & B12_MASK);
            printf("IF = %o\n", (pc & FIELD_MASK) >> 12);
          } else {
            printf("Syntax ERROR, IF can be between 0 and 03\n");
          }
          break;
        case E_PC:
          val = read_15bit_octal(_3rd_str);
          if( val >= 0 ){
            pc = val;
            printf("PC = %o\n", pc);
          }
          break;
        case OCTAL_LITERAL:
          addr = read_15bit_octal(_2nd_str);
          val = read_12bit_octal(_3rd_str);
          if( addr >= 0 && val >= 0 ){
            mem[addr] = val;
            print_instruction(addr);
          }
          break;
        default:
          printf("Syntax ERROR, deposit to what?\n");
        }
        break;
      case HELP:
        if( NULL_TOKEN != _3rd_tok ){
          to_many_args();
          break;
        }
        switch(_2nd_tok) {
        case BREAK:
          printf("\n  Set breakpoint at memory address\n\n"
                 "  break <octal value>\n\n"

                 "    Set or unset breakpoint add given address, the CPU will halt when\n"
                 "    the PC reaches an address with set breakpoint.\n\n"

                 "  break list\n\n"

                 "    List addresses of all set breakpoints.\n\n"

                 "  break clear\n\n"

                 "    Unset ALL breakpoints.\n\n");
          break;
        case RUN:
          printf("\n  Start CPU execution.\n\n"

                 "  run\n\n"

                 "    Execution is started at current PC. Input is directed at TTY.\n\n");
          break;
        case STEP:
          printf("\n  Execute one instruction.\n\n"

                 "  step\n\n"

                 "    print contents of CPU registers prior to executing the next\n"
                 "    instruction then print the instruction at PC. Interrupts are\n"
                 "    checked after the next instruction is printed so you might not know\n"
                 "    what was executed.\n\n");
          break;
        case TRACE:
          printf("\n  Print instruction trace\n\n"

                 "  trace\n\n"

                 "    Turn trace ON or OFF. When ON, each executed instruction is printed.\n\n");
          break;
        case DEPOSIT:
          printf("\n  No help yet :(\n\n");
          break;
        case EXAMINE:
          printf("\n  No help yet :(\n\n");
          break;
        case SAVE:
          printf("\n  No help yet :(\n\n");
          break;
        case RESTORE:
          printf("\n  No help yet :(\n\n");
          break;
        case TTY_ATTACH:
          printf("\n  No help yet :(\n\n");
          break;
        case TTY_SOURCE:
          printf("\n  No help yet :(\n\n");
          break;
        case NULL_TOKEN:
        default:
          printf("\n  Run control commands:\n\n"
                 "    (b)reak    (r)un    (s)tep    (t)race\n\n"
                 "  Memory control commands:\n\n"
                 "    (d)eposit    (e)xamine     (sa)ve    (re)store\n\n"
                 "  Device specific:\n\n"
                 "    (tty_a)ttach   (tty_s)ource\n\n"
                 "  Emulator control:\n\n"
                 "    (exit)\n\n"
                 "  Type \"help <command>\" for details.\n\n");
          // TODO help text for each command
          break;
        }
        break;
      case RUN:
        in_console = 0;
        done = 1;
        break;
      case SAVE:
        if( NULL_TOKEN != _3rd_tok ){
          to_many_args();
          break;
        }

        if( NULL_TOKEN != _2nd_tok ){
          int do_save=0;
          if(access(_2nd_str, F_OK) != -1){
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
          if(do_save && save_state(_2nd_str)){
            printf("CPU state saved\n");
          }
        } else {
          to_few_args();
        }
        break;
      case RESTORE:
        if( NULL_TOKEN != _3rd_tok ){
          to_many_args();
          break;
        }

        if( NULL_TOKEN != _2nd_tok ){
          if( ! restore_state(_2nd_str) ) {
            printf("Unable to restore state, state unchanged\n");
          } else {
            printf("CPU state restored\n");
          }
        } else {
          to_few_args();
        }
        break;
      case STEP:
        if( _2nd_tok != NULL_TOKEN ){
          to_many_args();
          break;
        }
        // TODO figure out how print useful information.
        print_regs();
        printf("\t\t");
        print_instruction(pc);
        in_console = 1;
        done = 1;
        break;
      case TRACE:
        if( _2nd_tok != NULL_TOKEN ){
          to_many_args();
          break;
        }

        trace_instruction = !trace_instruction;
        if( trace_instruction ){
          printf("Instruction trace ON\n");
        } else {
          printf("Instruction trace OFF\n");
        }
        break;
      case TTY_ATTACH:
        if( NULL_TOKEN != _3rd_tok ){
          to_many_args();
          break;
        }

        if( NULL_TOKEN != _2nd_tok ){
          if( tty_read_from_file ){
            printf("Unable to set new file name, tty_file currently open\n");
          } else {
            strncpy(tty_file, _2nd_str, strlen(_2nd_str)+1);
          }
        } else {
          to_few_args();
        }
        break;
      case TTY_SOURCE:
        if( _2nd_tok != NULL_TOKEN ){
          to_many_args();
          break;
        }

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
        break;
      case NULL_TOKEN:
        break;
      default:
        printf("Syntax ERROR, unknown command\n");
        break;
      }
    }
    free(line);
  }

  linenoiseHistorySave("history.txt");
  return in_console;
}


void exit_cleanup(void)
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

  memcpy(mem, rmem, MEMSIZE);
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
    int value;
    char *endptr;

    static struct option long_options[] = {
      {"restore",     required_argument, 0, 'r' },
      {"exit_on_HLT", no_argument,       0, 'e' },
      {"stop_at",     required_argument, 0, 's' },
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
      value = strtol(optarg, &endptr,8);
      if( errno == EINVAL || errno == ERANGE || *endptr != '\0' ){
        printf("?? stop_at argument must be octal ??\n");
        exit(EXIT_FAILURE);
      }
      if( value < 0 || value > 077777 ){
        printf("?? pc option out of range ??\n");
        exit(EXIT_FAILURE);
      }
      stop_at = value;
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
