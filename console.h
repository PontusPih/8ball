#ifndef _CONSOLE_H_
#define _CONSOLE_H_

void console_setup(int argc, char **argv);
char console_read_tty_byte(char *output);
void console_write_tty_byte(char output);
void console_stop_at(void);
void console_trace_instruction(void);

void console(void);

#endif // _CONSOLE_H_
