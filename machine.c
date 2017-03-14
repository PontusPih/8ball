#include "cpu.h"
#include "tty.h"
#include "serial_com.h"
#include "machine.h"

char com_buf_len = 0;
char com_buf[128];
char trace_instruction = 0;
short internal_stop_at = -1;

#ifdef PTY_SRV
#define _XOPEN_SOURCE 700

#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
int ptm = -1; // PTY master handle
#define UNUSED(x) (void)(x);
#include <string.h>
#endif

#ifdef PTY_CLI
#include "console.h"
#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#define _BSD_SOURCE 1
#define __USE_MISC 1
#include <termios.h>
int pts = -1; // PTY slave handle
#endif

#ifdef SERVER_BUILD
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "console.h"
#endif

void machine_setup(char *pty_name)
{
#ifdef SERVER_BUILD
  cpu_init();
  tty_reset();
#endif


#ifdef PTY_SRV
  UNUSED(pty_name); // To avoid warning.
  cpu_init();
  tty_reset();

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


#ifdef PTY_CLI
  if( pty_name == NULL ){
    printf("Please give a PTY name\n");
    exit(EXIT_FAILURE);
  }
  pts = open(pty_name, O_RDWR|O_NOCTTY);
  
  if( pts == -1 ){
    printf("Unable to open %s\n", pty_name);
    exit(EXIT_FAILURE);
  }
  
  struct termios cons_old_settings;
  struct termios cons_new_settings;
  tcgetattr(pts, &cons_old_settings);
  cons_new_settings = cons_old_settings;
  cfmakeraw(&cons_new_settings);
  tcsetattr(pts, TCSANOW, &cons_new_settings);
#endif
}


char read_tty_byte(char *output)
{
#ifdef PTY_SRV
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


void write_tty_byte(char output)
{
#ifdef PTY_SRV
  unsigned char buf[3] = { 'T', 'W', output };
  send_cmd(ptm, buf,3); // TTY wants to Write
#else
  console_write_tty_byte(output);
#endif
}


char machine_run(char single)
{
#if defined(PTY_SRV) || defined(SERVER_BUILD)
  int tty_skip_count = 0;
  while(1) {
#ifdef PTY_SRV
    if( recv_console_break(ptm) ){
      return 'I';
    }
#endif

    // This loops calls each emulated device in turn and any call that
    // uses recv_cmd() must return to console mode immediately if the
    // CONSOLE byte has been received.

    // Any device that can should be able to resume state if CONSOLE
    // has been recv:d

    if( tty_skip_count++ >= 100  ){ // TODO simulate slow TTY (update maindec-d0cc to do all loops)
      tty_skip_count = 0;
      if( tty_process() == -1 ){
        return 'I';
      }
    }
  
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

    if( trace_instruction ){
#ifdef PTY_SRV
      return 'D';
#else
      // TODO handle this in console.c
      console_trace_instruction();
#endif
    }
  }
#endif

#ifdef PTY_CLI
  unsigned char run_buf[1] = { single ? 'S' : 'R' };
  send_cmd(pts, run_buf, 1);

  while(1) {
    unsigned char *buf;
    recv_cmd(pts, &buf);
    switch(buf[0]) {
    case 'I': // Interrupted
      send_cmd(pts, (unsigned char*)"C",1);
    case 'H': // CPU has halted
    case 'B': // Breakpoint hit
    case 'S': // Single step done
    case 'P': // stop_at hit
      return buf[0];
      break;
    case 'T': // TTY Request
      if( buf[1] == 'R' ){
        char output;
        char res = console_read_tty_byte(&output);
        unsigned char buf[2] = { res, output };
        send_cmd(pts, buf, 2);
      } else {
        console_write_tty_byte(buf[2]);
      }
      break;
    case 'D': // Display (trace) instruction.
      console_trace_instruction();
      // TODO handle this in console.c
      // During trace instruction the server pops out to console mode,
      // restart it.
      send_cmd(pts, run_buf, 1);
      break;
    }
  }
#endif
}


short buf2short(unsigned char *b, int i)
{
  return (b[i] << 8) | (b[i+1] & 0xFF);
}


void send_short(short val)
{
  unsigned char buf[2] = { val >> 8, val & 0xFF };
#ifdef PTY_CLI
  send_cmd(pts, buf, 2);
#endif
#ifdef PTY_SRV
  send_cmd(ptm, buf, 2);
#endif
}


#ifdef PTY_SRV
int main()
{
  machine_setup(NULL);
  while(1){
    // First start in CONSOLE mode
    unsigned char *buf;
    recv_cmd(ptm, &buf); // TODO ack console break at any
                         // time. Because a new client will want to
                         // know current state.
    switch(buf[0]) {
    case 'R': // Start execution
    case 'S': // Single step
      {
        char single = buf[0] == 'S' ? 1 : 0;
        char state = machine_run(single);
        switch( state ){
        case 'I':
          buf[0] = 'I';
          send_cmd(ptm, buf, 1);
          while(1) {
            // system interrupted. await acknowledge
            recv_cmd(ptm, &buf);
            if(buf[0] == 'C'){
              break;
            }
          }
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
          res = machine_examine_reg(buf[2]);
          break;
        case 'M': // Memory
          res = machine_examine_mem(buf2short(buf,2));
          break;
        case 'O': // Operand addr
          res = machine_operand_addr(buf2short(buf,2), buf[4]);
          break;
        case 'D': // Direct addr
          res = machine_direct_addr(buf2short(buf,2));
          break;
        case 'B': // Breakpoint
          res = machine_examine_bp(buf2short(buf,2));
          break;
        case 'T': // Trace
          res = machine_examine_trace();
          break;
        }
        send_short(res);
      }
      break;
    case 'D': // Deposit
      switch(buf[1]){
      case 'R': // Register
        machine_deposit_reg(buf[2], buf2short(buf,3));
        break;
      case 'M': // Memory
        machine_deposit_mem(buf2short(buf,2), buf2short(buf,4));
        break;
      case 'B': // Breakpoint
        machine_toggle_bp(buf2short(buf, 2));
        break;
      case 'T': // Trace
        machine_toggle_trace();
        break;
      case 'P': // Stop at
        machine_set_stop_at(buf2short(buf,2));
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


short machine_examine_mem(short addr)
{
#ifdef PTY_CLI
  unsigned char buf[4] = { 'E', 'M', addr >> 8, addr & 0xFF };
  send_cmd(pts, buf, 4);
  unsigned char *rbuf;
  recv_cmd(pts, &rbuf);
  return buf2short(rbuf, 0);
#else
  return mem[addr];
#endif
}


void machine_deposit_mem(short addr, short val)
{
#ifdef PTY_CLI
  unsigned char buf[6] = { 'D', 'M', addr >> 8, addr & 0xFF, val >> 8, val & 0xFF };
  send_cmd(pts, buf, 6);
#else
  mem[addr] = val;
#endif
}


short machine_operand_addr(short addr, char examine)
{
#ifdef PTY_CLI
  unsigned char buf[5] = { 'E', 'O', addr >> 8, addr & 0xFF, examine };
  send_cmd(pts, buf, 5);
  unsigned char *rbuf;
  recv_cmd(pts, &rbuf);
  return buf2short(rbuf, 0);
#else
  return operand_addr(addr, examine);
#endif
}


short machine_direct_addr(short addr)
{
#ifdef PTY_CLI
  unsigned char buf[4] = { 'E', 'D', addr >> 8, addr & 0xFF };
  send_cmd(pts, buf, 4);
  unsigned char *rbuf;
  recv_cmd(pts, &rbuf);
  return buf2short(rbuf, 0);
#else
  return direct_addr(addr);
#endif
}


short machine_examine_deposit_reg(register_name_t reg, short val, char dep)
{
#ifdef PTY_CLI
  if( dep ){
    unsigned char buf[5] = { 'D', 'R', reg, val >> 8, val & 0xFF };
    send_cmd(pts,buf,5);
    return 0;
  } else {
    unsigned char buf[3] = { 'E', 'R', reg };
    send_cmd(pts,buf,3);
    unsigned char *rbuf;
    recv_cmd(pts,&rbuf);
    return buf2short(rbuf, 0);
  }

#else

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
#ifdef SERVER_BUILD
  default:
    printf("OOPS, unknown reg, |%d|", reg);
    exit(EXIT_FAILURE);
#endif
  }

  return res;
#endif
}


short machine_examine_reg(register_name_t regname)
{
  return machine_examine_deposit_reg(regname, 0, 0);
}


void machine_deposit_reg(register_name_t regname, short val)
{
  machine_examine_deposit_reg(regname, val, 1);
}


void machine_clear_all_bp()
{
#ifdef PTY_BUILD
  // TODO Clear all breakpoints
#endif

#ifdef SERVER_BUILD
  memset(breakpoints, 0, MEMSIZE);
#endif

#ifdef SERIAL_BUILD
#endif
}


short machine_examine_bp(short addr)
{
#ifdef PTY_CLI
  unsigned char buf[4] = { 'E', 'B', addr >> 8, addr & 0xFF };
  send_cmd(pts, buf, 4);
  unsigned char *rbuf;
  recv_cmd(pts, &rbuf);
  return buf2short(rbuf, 0);
#else
  return breakpoints[addr];
#endif
}


void machine_toggle_bp(short addr)
{
#ifdef PTY_CLI
  unsigned char buf[4] = { 'D', 'B', addr >> 8, addr & 0xFF };
  send_cmd(pts, buf, 4);
#else
  breakpoints[addr] = breakpoints[addr] ^ BREAKPOINT;
#endif
}


short machine_examine_trace()
{
#ifdef PTY_CLI
  unsigned char buf[2] = { 'E', 'T' };
  send_cmd(pts, buf, 2);
  unsigned char *rbuf;
  recv_cmd(pts, &rbuf);
  return rbuf[1]; // Result sent as a short, least significant byte contains boolean.
#else
  return trace_instruction;
#endif
}


void machine_toggle_trace()
{
#ifdef PTY_CLI
  unsigned char buf[2] = { 'D', 'T' };
  send_cmd(pts, buf, 2);
#else
  trace_instruction = !trace_instruction;
#endif
}


void machine_set_stop_at(short addr)
{
#ifdef PTY_CLI
  unsigned char buf[4] = { 'D', 'P', addr >> 8, addr & 0xFF };
  send_cmd(pts, buf, 4);
#else
  internal_stop_at = addr;
#endif
}


void machine_quit()
{
#ifdef PTY_CLI
  unsigned char buf[1] = { 'Q' };
  send_cmd(pts, buf, 1);
#endif
}


void machine_interrupt()
{
#ifdef PTY_CLI
  send_console_break(pts);
  send_console_break(pts);
#endif

  // TODO server build?
}
