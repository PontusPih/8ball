#ifndef _CONSOLE_H_
#define _CONSOLE_H_

void console_setup(int argc, char **argv);
void console_stop_break(void);
void console_trace_instruction(void);
char read_tty_byte(char *output);

#ifdef HOST_BUILD
extern char in_console;
extern char exit_on_HLT;
char console(void);
#else
char in_console = 1;
int console(void){
  return 0;
}
#endif

#endif // _CONSOLE_H_
