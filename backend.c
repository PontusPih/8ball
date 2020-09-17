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
int interrupted_by_console = 0;
char trace_instruction = 0;
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
  return recv_console_break(ptm);
#else
  return interrupted_by_console;
#endif
}

char backend_run(char single)
{
  while(1){
    if( backend_interrupted() ){
      return 'I';
    }
    
    if( trace_instruction ){
      return 'D';
    }
    
    // This loops calls each emulated device in turn and any call that
    // uses recv_cmd() must return to console mode immediately if the
    // CONSOLE byte has been received.
    
    // Any device that can should be able to resume state if CONSOLE
    // has been recv:d
    
    if( single || tty_skip_count++ >= 100  ){ // TODO simulate slow TTY (update maindec-d0cc to do all loops)
      tty_skip_count = 0;
      if( tty_process() == -1 ){
	return 'I';
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


#ifdef PTY_CLI

void ack_console()
{
  unsigned char buf[1] = { 'I' };
  send_cmd(ptm, buf, 1);
  while(1) {
    // system interrupted. await acknowledge
    unsigned char *rbuf;
    if( recv_cmd(ptm, &rbuf) > 0 && rbuf[0] == 'C'){
      break;
    }
  }
}


short buf2short(unsigned char *b, int i)
{
  return (b[i] << 8) | (b[i+1] & 0xFF);
}


void send_short(short x)
{
  unsigned char buf[2] = { x >> 8, x & 0xFF };
  send_cmd(ptm, buf, 2);
}


int main(int argc, char **argv)
{
  //  UNUSED(argc);
  //  UNUSED(argv);
  backend_setup(NULL);
  while(1){
    // First start in CONSOLE mode
    unsigned char *buf;
    if( recv_cmd(ptm, &buf) < 0 ) {
      ack_console(); // TODO BUG. no console commands expect an ack
      continue; // Received break and acked it, get next command.
    }
    switch(buf[0]) {
    case 'R': // Start execution
    case 'S': // Single step
      {
        char single = buf[0] == 'S' ? 1 : 0;
        char state = backend_run(single);
        switch( state ){
        case 'I':
          ack_console(); // Wait for ack.
          break;
        default:
          buf[0] = state;
          send_cmd(ptm, buf, 1);
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
        case 'T': // Trace
          res = backend_examine_trace();
          break;
        }
        send_short(res);
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
      case 'T': // Trace
        backend_toggle_trace();
        break;
      case 'P': // Stop at
        backend_set_stop_at(buf2short(buf,2));
        break;
      case 'X': // RX byte stream
	// TODO serial com support for RX
	break;
      }
      break;
    case 'Q':
      close(ptm);
      exit(EXIT_SUCCESS);
    default:
      // TODO send 'U' for unknown command.
      printf("Unkown coms: %s\n", buf);
      exit(EXIT_FAILURE);
    }
  }
}
#endif

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


short backend_examine_trace()
{
  return trace_instruction;
}


void backend_toggle_trace()
{
  trace_instruction = !trace_instruction;
}


void backend_set_stop_at(short addr)
{
  internal_stop_at = addr;
}


void backend_interrupt()
{
  interrupted_by_console = 1;
}


char backend_read_tty_byte(char *output)
{
#ifdef PTY_CLI
  unsigned char buf[2] = { 'T', 'R' };
  send_cmd(ptm, buf,2); // TTY wants to Read
  unsigned char *rbuf;
  if(recv_cmd(ptm, &rbuf) > 0 ){
    *output = rbuf[1];
    return rbuf[0];
  } else {
    return -1;
  }
#else
  return console_read_tty_byte(output);
#endif
}


void backend_write_tty_byte(char output)
{
#ifdef PTY_CLI
  unsigned char buf[3] = { 'T', 'W', output };
  send_cmd(ptm, buf,3); // TTY wants to Write
#else
  console_write_tty_byte(output);
#endif
}
