#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "linenoise.h"

#define MEMSIZE 0100000
#define FIELD_MASK 070000
#define PAGE_MASK 07600
#define WORD_MASK 0177
#define B12_MASK 07777
#define B8_MASK 0377
#define IF_MASK 07000
#define LINK_MASK 010000
#define LINK_AC_MASK 017777
#define Z_MASK 00200
#define I_MASK 00400
#define SIGN_BIT_MASK 04000
#define LINK (ac & LINK_MASK)
#define DEV_MASK 0770
#define IOT_OP_MASK 07

#define INSTR(x) ((x)<<9)
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

short mem[MEMSIZE];

// CPU registers
short pc = 0200;
short df;
short ac = 0;
short mq;
short sr = 05252;

// TTY registers
short tty_kb_buf = 0;
short tty_kb_flag = 0;
short tty_tp_buf = 0;
short tty_tp_flag = 1;
short tty_dcr = 0; // device control register

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

// termios globals
struct termios told, tnew;

void completion_cb(const char *buf, linenoiseCompletions *lc);
char console();

short operand_addr(short pc){
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
  
  if( cur & I_MASK ){
    // indirect addressing
    if( (addr & (PAGE_MASK|WORD_MASK)) >= 010 
	&& 
	(addr & (PAGE_MASK|WORD_MASK)) <= 017 ){
      // autoindex addressing
      mem[addr] = (mem[addr]+1) & B12_MASK;
    }
    addr = mem[addr];
  }
  return addr;
}

int main ()
{
    //#include "hello_world.h"
    //    pc = 0200;

#include "rimloader.h"
    pc = 07756;

  // Setup console
  /* Set the completion callback. This will be called every time the
   * user uses the <tab> key. */
  linenoiseSetCompletionCallback(completion_cb);
  
  /* Load history from file. The history file is just a plain text file
   * where entries are separated by newlines. */
  /* linenoiseHistoryLoad("history.txt"); / * Load the history at startup */
  
  // Setup terminal for canonical mode:
  
  tcgetattr(0,&told);
  tnew = told;
  tnew.c_lflag &= ~(ICANON|ECHO);
  tnew.c_cc[VMIN] = 0; // Allow read() to return without data.
  tnew.c_cc[VTIME] = 0; // But block for one tenth of a second.
  tcsetattr(0, TCSANOW, &tnew);

  char in_console = 1;

  while(1){

    // TTY and console handling:
    // TODO, interruptenable
    char input;
    char nchar = read(0, &input, 1);
    if( nchar || in_console || tty_read_from_file ){
        if( input == 033 || in_console ){
            in_console = console();
        }
        
        if( (nchar || tty_read_from_file) && !tty_kb_flag ){
            // If keyboard flag is not set, try to read one char.
            if( tty_read_from_file ){
                int byte = fgetc( tty_fh );
                printf( "read %d from file\n", byte);
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
            }
        }
    }

    // TODO, proper DF-handling
    short cur = *(mem+pc);
    short addr = operand_addr(pc);

    pc = INC_PC(pc); // PC is incremented after fetch, so JMP works :)
    
    switch( cur & IF_MASK ){
    case AND:
      // AND AC and operand.
      ac &= mem[addr];
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
	pc = INC_PC(pc)
      }
      break;
    case DCA:
      // Deposit and Clear AC
      mem[addr] = (ac & B12_MASK);
      ac = 0;
      break;
    case JMS:
      // Jump and store return address.
      mem[addr] = (pc & B12_MASK);
      pc = (pc & FIELD_MASK) | ((addr + 1) & B12_MASK);
      break;
    case JMP:
      // Unconditional Jump
      pc = addr;
      break;
    case IOT:
      switch( (cur & DEV_MASK) >> 3 ){
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
	  tty_dcr = ac & 01; // Write IE bit of ac to DCR (SE not supported).
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
	  break;
	case TSF:
	  if( tty_tp_flag ){
	    pc = INC_PC(pc);
	  }
	  break;
	case TCF:
	  tty_tp_flag = 0;
	  break;
	case TPC:
	  tty_tp_buf = (ac & B8_MASK);
	  write(1, &tty_tp_buf, 1);
	  // TODO "async" output?
	  tty_tp_flag = 1;
	  break;
	case TSK:
	  if( tty_tp_flag || tty_kb_flag ){
	    pc = INC_PC(pc);
	  }
	  break;
	case TLS:
	  tty_tp_flag = 0;
	  tty_tp_buf = (ac & B8_MASK);
	  write(1, &tty_tp_buf, 1);
	  // TODO "async" output?
	  tty_tp_flag = 1;
	  break;
	default:
	  printf("illegal IOT instruction. device 04 - TTY output\n");
	  in_console = 1;
	  break;
	}
	break;
      default:
	printf("Illegal IOT instruction, unknown device");
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
	  ac &= B12_MASK;
	}
	
	if( cur & CMA ){
	  // Complement AC
	  ac = ac ^ B12_MASK;
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
	  char msb = (ac & 07700) >> 6;
	  char lsb = (ac & 00077) << 6;
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
	    
	    if( cur & SZA && ! (ac & B12_MASK) ){
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
	    
	    if( cur & SNA && !(ac & B12_MASK) ){
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
            printf(">>> CPU HALTED <<<\n");
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
	    mq = ac & B12_MASK;
	    ac = tmp;
	  } else {
	    // Otherwise apply MQA or MQL separately
	    if( cur & MQA ){
	      ac = (ac & B12_MASK) | (mq & B12_MASK);
	    }
	    
	    if( cur & MQL ){
	      mq = ac & B12_MASK;
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
  //  short addr = operand_addr(pc);

  printf("%.5o  %.5o", pc, cur);

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
    if( cur & I_MASK )
      printf(" I");
    if( cur & Z_MASK )
      printf(" Z");
    printf(" %.4o", cur & WORD_MASK);
  } else {
    switch( cur & IF_MASK ){
    case IOT:
      printf(" IOT");
      break;
    case OPR:
      printf(" OPR");
      // TODO print more details.
      break;
    }
  }
  printf("\n");
}

void completion_cb(const char *buf, linenoiseCompletions *lc){

}

short read_12bit_octal(const char *buf)
{
    unsigned int res;
    sscanf(buf, "%o", &res);
    if( res > B12_MASK ){
        printf("Octal value to large, truncated: %.5o\n", res & B12_MASK);
    }
    return (short)(res & B12_MASK);
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
	print_instruction(pc);
	in_console = 1;
	done = 1;
      }

      if( ! strncasecmp(skip_line, "r\0", 2) ){
	in_console = 0;
	done = 1;
      }
      
      if( ! strncasecmp(skip_line, "d\0", 2) ){
	printf("PC = %o AC = %o MQ = %o DF = %o SR = %o\n", pc, ac, mq, df, sr);
      }

      if( ! strncasecmp(skip_line, "e ", 2) ){
          skip_line += 2;
          int start = read_12bit_octal(skip_line);
          while( *skip_line != ' ' ){
              skip_line++;
          }

          int end = read_12bit_octal(skip_line);

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
      
      if( ! strncasecmp(skip_line, "exit", 4) ){
	tcsetattr(0, TCSANOW, &told);
	exit(0);
      }
      
    } 
    free(line);
  }
  
  return in_console;
}
