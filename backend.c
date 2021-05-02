#ifdef PTY_CLI
#define _XOPEN_SOURCE 700
#include <fcntl.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int ptm = -1; // PTY master handle
#else
#include "console.h"
#endif

#include "cpu.h"
#include "tty.h"
#include "rx8.h"
#include "serial_com.h"
#include "machine.h"
#include "backend.h"

int tty_skip_count = 0;
char tty_output_pending = 0;
char tty_output_sent = 0;
char tty_output_char = '\0';

char tty_input_wanted = 0;
char tty_input_received = 0;
char tty_input_char = '\0';

int interrupted_by_console = 0;
short internal_stop_at = -1;

void backend_setup()
{
  cpu_init();
  tty_reset();
  rx8e_reset();

#ifdef PTY_CLI
  if( (ptm = posix_openpt(O_RDWR|O_NOCTTY)) == -1){
    printf("Unable to open PTMX\n");
    exit(EXIT_FAILURE);
  }

  if( grantpt(ptm) == -1 ){
    printf("grantp() failed\n");
    exit(EXIT_FAILURE);
  }

  if( unlockpt(ptm) == -1 ){
    printf("Unable to unlock PTY\n");
    exit(EXIT_FAILURE);
  }

  int fd = creat("ptsname.txt", S_IRUSR | S_IWUSR);
  write(fd, ptsname(ptm), strlen(ptsname(ptm)));
  close(fd);
#endif
}


int backend_interrupted()
{
#ifdef PTY_CLI
  if( recv_console_break(ptm) ){
    interrupted_by_console = 1;
  }
#endif
  return interrupted_by_console;
}

char backend_run(char single)
{
  while(1){
    if( backend_interrupted() ){
      return 'I';
    }
    
    // This loops calls each emulated device in turn and any call that
    // uses recv_cmd() must return to console mode immediately if the
    // CONSOLE byte has been received.
    
    // Any device that can should be able to resume state if CONSOLE
    // has been recv:d
    
    if( tty_skip_count++ >= 100  ){ // TODO simulate slow TTY (update maindec-d0cc to do all loops)
      // Current implementation requires at least one skip_count
      // otherwise single stepping goes tits up
      tty_skip_count = 0;
      if( tty_process() ){
	return 'T';
      }
    }
    
    rx01_process();
    
    if( cpu_process() == -1 ){
      return 'H';
    }
    
    if( breakpoints[pc] & BREAKPOINT ){
      return 'B';
    }
    
    if( internal_stop_at >= 0 && pc == internal_stop_at ){
      return 'P';
    }      
    
    if( single ){
      return 'S';
    }
  }
}


short buf2short(unsigned char *b, int i)
{
  return (b[i] << 8) | (b[i+1] & 0xFF);
}


void backend_dispatch(unsigned char *buf, unsigned char *reply_buf, int *reply_length)
{
  switch(buf[0]) {
  case 'R': // Start execution
    // printf("Running CPU\n");
    __attribute__ ((fallthrough));
  case 'C': // Continue running
    __attribute__ ((fallthrough));
  case 'S': // Single step
    {
      char single = buf[0] == 'S' ? 1 : 0;
      char state = backend_run(single);

      switch( state ){
      case 'T':
	// TTY wants something
	if( tty_output_pending ){
	  reply_buf[0] = 'T';
	  reply_buf[1] = 'W'; // TTY wants to Write
	  reply_buf[2] = tty_output_char;
	  *reply_length = 3;
	  tty_output_sent = 1;
	} else if( tty_input_wanted ){
	  reply_buf[0] = 'T';
	  reply_buf[1] = 'R'; // TTY wants to Read
	  *reply_length = 2;
	}
	break;
      case 'R':
	// RX wants something
	break;
      default:
	reply_buf[0] = state;
	*reply_length = 1;
	break;
      }
    }
    break;
  case 'E': // Examine
    {
      short res;
      switch(buf[1]){
      case 'R': // Register
	res = backend_examine_deposit_reg(buf[2], 0, 0);
	break;
      case 'M': // Memory
	res = mem[buf2short(buf,2)];
	break;
      case 'O': // Operand addr
	res = operand_addr(buf2short(buf,2), buf[4]);
	break;
      case 'D': // Direct addr
	res = direct_addr(buf2short(buf,2));
	break;
      case 'B': // Breakpoint
	res = backend_examine_bp(buf2short(buf,2));
	break;
      }
      reply_buf[0] = 'V';
      reply_buf[1] = res >> 8;
      reply_buf[2] = res & 0xFF;
      *reply_length = 3;
    }
    break;
  case 'D': // Deposit
    switch(buf[1]){
    case 'R': // Register
      backend_examine_deposit_reg(buf[2], buf2short(buf,3), 1);
      break;
    case 'M': // Memory
      //printf("%o = %o\n", buf2short(buf,2) ,  buf2short(buf,4));
      mem[buf2short(buf,2)] = buf2short(buf,4);
      break;
    case 'B': // Breakpoint
      backend_toggle_bp(buf2short(buf, 2));
      break;
    case 'P': // Stop at
      backend_set_stop_at(buf2short(buf,2));
      break;
    case 'X': // RX byte stream
      // TODO serial com support for RX
      break;
    }
    reply_buf[0] = 'A'; // ACKnowledge communication
    *reply_length = 1;
    break;
  case 'T':
    tty_input_received = 1;
    tty_input_char = buf[1];
    reply_buf[0] = 'A'; // ACKnowledge communication
    *reply_length = 1;
    break;
  default:
    reply_buf[0] = 'U';
    *reply_length = 1;
    break;
  }
}


short backend_examine_deposit_reg(register_name_t reg, short val, char dep)
{
  short res = 0;
  
  switch( reg ){
  case AC:
    if( dep ){
      ac = val;
    }
    res = ac;
    break;
  case PC:
    if( dep ){
      pc = val;
    }
    res = pc;
    break;
  case MQ:
    if( dep ){
      mq = val;
    }
    res = mq;
    break;
  case DF:
    if( dep ){
      df = val;
    }
    res = df;
    break;
  case IB:
    if( dep ){
      ib = val;
    }
    res = ib;
    break;
  case UB:
    if( dep ){
      ub = val;
    }
    res = ub;
    break;
  case UF:
    if( dep ){
      uf = val;
    }
    res = uf;
    break;
  case SF:
    if( dep ){
      sf = val;
    }
    res = sf;
    break;
  case SR:
    if( dep ){
      sr = val;
    }
    res = sr;
    break;
  case ION_FLAG:
    if( dep ){
      ion = val;
    }
    res = ion;
    break;
  case ION_DELAY:
    if( dep ){
      ion_delay = val;
    }
    res = ion_delay;
    break;
  case INTR_INHIBIT:
    if( dep ){
      intr_inhibit = val;
    }
    res = intr_inhibit;
    break;
  case INTR:
    if( dep ){
      intr = val;
    }
    res = intr;
    break;
  case RTF_DELAY:
    if( dep ){
      rtf_delay = val;
    }
    res = rtf_delay;
    break;
  case TTY_KB_BUF:
    if( dep ){
      tty_kb_buf = val;
    }
    res = tty_kb_buf;
    break;
  case TTY_KB_FLAG:
    if( dep ){
      tty_kb_flag = val;
    }
    res = tty_kb_flag;
    break;
  case TTY_TP_BUF:
    if( dep ){
      tty_tp_buf = val;
    }
    res = tty_tp_buf;
    break;
  case TTY_TP_FLAG:
    if( dep ){
      tty_tp_flag = val;
    }
    res = tty_tp_flag;
    break;
  case TTY_DCR:
    if( dep ){
      tty_dcr = val;
    }
    res = tty_dcr;
    break;
  case RX_IR:
    if( dep ){
      rx_ir = val;
    }
    res = rx_ir;
    break;
  case RX_TR:
    if( dep ){
      rx_tr = val;
    }
    res = rx_tr;
    break;
  case RX_DF:
    if( dep ){
      rx_df = val;
    }
    res = rx_df;
    break;
  case RX_EF:
    if( dep ){
      rx_ef = val;
    }
    res = rx_ef;
    break;
  case RX_ONLINE:
    if( dep ){
      rx_online = val;
    }
    res = rx_online;
    break;
  case RX_BIT_MODE:
    if( dep ){
      rx_bit_mode = val;
    }
    res = rx_bit_mode;
    break;
  case RX_MAINTENANCE_MODE:
    if( dep ){
      rx_maintenance_mode = val;
    }
    res = rx_maintenance_mode;
    break;
  case RX_INTR_ENABLED:
    if( dep ){
      rx_intr_enabled = val;
    }
    res = rx_intr_enabled;
    break;
  case RX_RUN:
    if( dep ){
      rx_run = val;
    }
    res = rx_run;
    break;
  case RX_FUNCTION:
    if( dep ){
      current_function = val;
    }
    res = current_function;
    break;
  case RX_READY_0:
    if( dep ){
      rx_ready[0] = val;
    }
    res = rx_ready[0];
    break;
  case RX_READY_1:
    if( dep ){
      rx_ready[1] = val;
    }
    res = rx_ready[1];
    break;
#ifdef PTY_CLI
  default:
    printf("OOPS, unknown reg, |%d|", reg);
    exit(EXIT_FAILURE);
#endif
  }

  return res;
}


void backend_clear_all_bp()
{
  for(int i = 0; i<= MEMSIZE; i++){
    breakpoints[i] = 0;
  }
}


short backend_examine_bp(short addr)
{
  return breakpoints[addr];
}


void backend_toggle_bp(short addr)
{
  breakpoints[addr] = breakpoints[addr] ^ BREAKPOINT;
}


void backend_set_stop_at(short addr)
{
  internal_stop_at = addr;
}


void backend_interrupt()
{
  interrupted_by_console = 1;
}


char backend_read_tty_byte(char *input)
{
  if( tty_input_wanted && tty_input_received ){
    tty_input_wanted = tty_input_received = 0;
    *input = tty_input_char;
    return 1;
  } else {
    tty_input_wanted = 1;
    return 0;
  }
}


char backend_write_tty_byte(char output)
{
#ifdef PTY_CLI
  if( tty_output_pending && tty_output_sent ){
    // There was a request to write, and has been sent to frontend
    tty_output_pending = tty_output_sent = 0;
    return 1;
  } else {
    // Queue char for sending.
    tty_output_pending = 1;
    // Notice that the separate tty_output_sent flag allows the output
    // char to change should this function be called before output is
    // sent to the frontend. In practice it is sent immediately
    tty_output_char = output;
    return 0;
  }
#else
  machine_write_tty_byte(output);
  return 1;
#endif
}


#ifdef PTY_CLI
int main(__attribute__((unused)) int argc, __attribute__((unused)) char **argv)
{
  backend_setup(NULL);
  while(1){
    // First start in CONSOLE mode
    unsigned char buf[128];
    if( recv_cmd(ptm, buf) < 0 ) {
      send_console_break(ptm);
      interrupted_by_console = 1;
      continue; // Received break and acked it, get next command
    }

    if( interrupted_by_console ) {
      if( buf[0] == 'C' ){
	continue; // When interrupted, ignore continue commands.
      }
      if( buf[0] == 'R' || buf[0] == 'S' ){
	 // Run or single step command means we are no longer
	 // interrupted.
	interrupted_by_console = 0;
      }
    }

    unsigned char reply_buf[3]; // Max length is three so far :)
    int reply_length = 0;
    backend_dispatch(buf, reply_buf, &reply_length);

    if( reply_length > 0){
      send_cmd(ptm, reply_buf, reply_length);
    } else {
      printf( "BAD STATE, no reply to console\n");
      exit(EXIT_FAILURE);
    }
  }
}
#endif
