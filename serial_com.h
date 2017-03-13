#ifndef _SERIAL_COM_H_
#define _SERIAL_COM_H_

void send_cmd(int fd, unsigned char *cmd, int len);
int recv_cmd(int fd, unsigned char **out_buf);
void send_console_break(int fd);
char recv_console_break(int fd);

#endif // _SERIAL_COM_H_
