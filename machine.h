#ifndef _MACHINE_H_
#define _MACHINE_H_

typedef enum register_name {
  AC,
  PC,
  CPMA,
  MQ,
  DF,
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
} register_name_t;

short machine_examine_mem(short addr);
void machine_deposit_mem(short addr, short val);
short machine_operand_addr(short addr, char examine);
short machine_direct_addr(short addr);
short machine_examine_reg(register_name_t regname);
void machine_deposit_reg(register_name_t regname, short val);
void machine_clear_all_bp(void);
short machine_examine_bp(short addr);
void machine_toggle_bp(short addr);
short machine_examine_trace();
void machine_toggle_trace();
void machine_set_stop_at(short addr);
void machine_interrupt();
void machine_quit();
void machine_srv();

char read_tty_byte(char *output);
void write_tty_byte(char output);

void machine_setup(char *pty_name);
char machine_run(char single);
void machine_halt();

#endif // _MACHINE_H_
