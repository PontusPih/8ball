#include "console.h"
#include "cpu.h"
#include "tty.h"
#include "rx8.h"
#include "serial_com.h"
#include "backend.h"

// TODO simulate slow TTY (update maindec-d0cc to do all loops)
// Current implementation requires at least one skip_count
// otherwise single stepping goes tits up
int tty_kb_skip_count = 0;
int tty_tp_skip_count = 0;
char tty_output_char = '\0';

int interrupted_by_console = 1;
short internal_stop_at = -1;

void backend_toggle_bp(short addr);
short backend_examine_bp(short addr);
void backend_set_stop_at(short addr);
short backend_examine_deposit_reg(register_name_t reg, short val, char dep);

char backend_setup()
{
  cpu_init();
  tty_reset();
  rx8e_reset();

  return 1;
}


int backend_interrupted()
{
#ifdef PTY_CLI
  if( serial_recv_break() ){
    interrupted_by_console = 1;
  }
#endif
  return interrupted_by_console;
}

typedef enum device_name {
  KB,
  TP,
  RX,
  CPU
} device_name_t;

void backend_run(char single, unsigned char *reply_buf, int *reply_length)
{
  static device_name_t devices[4] = { CPU, KB, TP, RX };
  static int cur_device = 3;
  static int num_devices = 4;
  *reply_length = 1; // Modify for longer replies

  while(1){
    if( backend_interrupted() ){
      reply_buf[0] = 'I';
      return;
    }
    
    // This loops calls each emulated device in turn and acts on any
    // I/O activity.
    cur_device = (cur_device + 1) % num_devices;
    switch( devices[cur_device] ){
    case KB:
      if( tty_kb_skip_count++ >= 50 ) {
	tty_kb_skip_count = 0;
	if( ! tty_kb_flag ){
	  // If keyboard flag is not set, request one char.
	  reply_buf[0] = 'T';
	  reply_buf[1] = 'R'; // TTY wants to Read
	  *reply_length = 2;
	  return;
	}
      }
      break;
    case TP:
      if( tty_tp_skip_count++ >= 50 ) {
	tty_tp_skip_count = 0;
	if( tty_tp_process(&tty_output_char) ){
	  reply_buf[0] = 'T';
	  reply_buf[1] = 'W'; // TTY wants to Write
	  reply_buf[2] = tty_output_char;
	  *reply_length = 3;
	  return;
	}
      }
      break;
    case RX:
      rx01_process();
      break;
    case CPU:
      if( cpu_process() == -1 ){
	reply_buf[0] = 'H';
	return;
      }

      if( cpu_get_breakpoint(pc) ){
	reply_buf[0] = 'B';
	return;
      }

      if( internal_stop_at >= 0 && pc == internal_stop_at ){
	reply_buf[0] = 'P';
	return;
      }

      if( single ){
	reply_buf[0] = 'S';
	return;
      }
      break;
    default:
      // Unknown device type
      reply_buf[0] = 'E';
      return;
    }
  }
}


short buf2short(unsigned char *b, int i)
{
  return (b[i] << 8) | (b[i+1] & 0xFF);
}


void backend_dispatch(unsigned char *buf, __attribute__ ((unused)) int send_length, unsigned char *reply_buf, int *reply_length)
{
  switch(buf[0]) {
  case 'R': // Start execution
    __attribute__ ((fallthrough));
  case 'S': // Single step
    // Run or single step command means cpu no longer interrupted.
    interrupted_by_console = 0;
    __attribute__ ((fallthrough));
  case 'C': // Continue running
    {
      char single = buf[0] == 'S' ? 1 : 0;
      backend_run(single, reply_buf, reply_length);
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
	res = mem_read(buf2short(buf,2));
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

      if( buf[1] == 'M' && (res & (~B12_MASK)) ) {
	// No memory value should be outside of 12 bits, something
	// went wrong
	reply_buf[0] = 'E';
	*reply_length = 1;
      } else {
	reply_buf[0] = 'V';
	reply_buf[1] = res >> 8;
	reply_buf[2] = res & 0xFF;
	*reply_length = 3;
      }
    }
    break;
  case 'D': // Deposit
    switch(buf[1]){
    case 'R': // Register
      backend_examine_deposit_reg(buf[2], buf2short(buf,3), 1);
      break;
    case 'M': // Memory
      mem_write(buf2short(buf,2), buf2short(buf,4));
      break;
    case 'B': // Breakpoint
      backend_toggle_bp(buf2short(buf, 2));
      break;
    case 'P': // Stop at
      backend_set_stop_at(buf2short(buf,2));
      break;
    case 'C':
      backend_clear_all_bp();
      break;
    case 'X': // RX byte stream
      // TODO serial com support for RX
      reply_buf[0] = 'E';
      *reply_length = 1;
      break;
    }
    reply_buf[0] = 'A'; // ACKnowledge communication
    *reply_length = 1;
    break;
  case 'T':
    // Got char for TTY Keyboard
    tty_kb_process(buf[1]);
    reply_buf[0] = 'A'; // ACKnowledge communication
    *reply_length = 1;
    break;
  default:
    reply_buf[0] = 'E'; // Unknown command, raise Error TODO
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
    res = (pc & B12_MASK);
    break;
  case MQ:
    if( dep ){
      mq = val;
    }
    res = mq;
    break;
  case DF:
    if( dep ){
      df = val & 03;
    }
    res = df;
    break;
  case IF:
    if( dep ){
      pc = (((val << 12 ) & FIELD_MASK) | (pc & B12_MASK));
    }
    res = (pc & FIELD_MASK) >> 12;
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
  default:
    // Unknown register, no way of telling frontend TODO
    res = -1;
    break;
  }

  return res;
}


void backend_clear_all_bp()
{
  for(int i = 0; i<= MEMSIZE; i++){
    cpu_set_breakpoint(i, 0);
  }
}


short backend_examine_bp(short addr)
{
  return cpu_get_breakpoint(addr);
}


void backend_toggle_bp(short addr)
{
  cpu_set_breakpoint(addr, cpu_get_breakpoint(addr) ? 0 : 1);
}


void backend_set_stop_at(short addr)
{
  internal_stop_at = addr;
}


void backend_interrupt()
{
  interrupted_by_console = 1;
}


#ifdef PTY_CLI
int main(__attribute__((unused)) int argc, __attribute__((unused)) char **argv)
{
  backend_setup();
  serial_setup(0); // TODO remove and handle in Makefile
  while(1){
    // First start in CONSOLE mode
    unsigned char buf[128];
    int send_length = serial_recv(buf);
    if( send_length < 0 ) {
      serial_send_break();
      interrupted_by_console = 1;
      continue; // Received break and acked it, get next command
    }

    if( interrupted_by_console ) {
      if( buf[0] == 'C' ){
	continue; // When interrupted, ignore continue commands.
      }
    }

    unsigned char reply_buf[3]; // Max length is three so far :)
    int reply_length = 0;
    backend_dispatch(buf, send_length, reply_buf, &reply_length);
    serial_send(reply_buf, reply_length);
  }
}
#endif
