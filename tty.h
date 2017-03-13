#ifndef _TTY_H_
#define _TTY_H_

// TTY registers
extern short tty_kb_buf;
extern short tty_kb_flag;
extern short tty_tp_buf;
extern short tty_tp_flag;
extern short tty_dcr; // device control register

#define TTY_SE_MASK 02
#define TTY_IE_MASK 01

void tty_reset(void);
char tty_process(void);
void tty_initiate_output();

#endif // _TTY_H_
