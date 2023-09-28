#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "serial_com_ext.h"

int tty = -1; // TTY handle

// "blocking" write of single byte
void write_byte(char byte)
{
  ssize_t rlen = write(tty, &byte, 1);

  if( rlen < 1 ){
    printf("Unable to write to TTY\n");
    if( rlen < 0 ){
      perror(__func__);
    }
    exit(EXIT_FAILURE);
  }
}


// Search for specific character, if any, return true if found
char find_byte_nonblocking(char target)
{
  char byte;
  ssize_t len = 0;
  int flags = fcntl(tty, F_GETFL, 0);
  fcntl(tty, F_SETFL, flags | O_NONBLOCK);
  do {
    len = read(tty, &byte, 1);
  } while ( len == 1 && byte != target );
  fcntl(tty, F_SETFL, flags);

  return len == 1 && byte == target;
}


// "blocking" read of one byte
char read_byte()
{
  char byte;
  ssize_t len = 0;
  do {
    len = read(tty, &byte, 1);
  } while( len == 0 );

  if( len < 0 ){
    printf("Unable to read from TTY\n");
    perror(__func__);
    exit(EXIT_FAILURE);
  }
  return byte;
}


void channel_setup(char* tty_name)
{
  tty = open(tty_name, O_RDWR|O_NOCTTY);

  if( tty == -1 ){
    printf("Unable to open %s\n", tty_name);
    exit(EXIT_FAILURE);
  }

  struct termios cons_old_settings;
  struct termios cons_new_settings;
  tcgetattr(tty, &cons_old_settings);
  cons_new_settings = cons_old_settings;

  /*
  tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity
  tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used
  tty.c_cflag &= ~CSIZE; // Clear all the size bits, then set one below  
  tty.c_cflag |= CS8; // 8 bits per byte

  tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control
  tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

  tty.c_lflag &= ~ICANON;
  tty.c_lflag &= ~ECHO; // Disable echo
  tty.c_lflag &= ~ECHOE; // Disable erasure
  tty.c_lflag &= ~ECHONL; // Disable new-line echo
  tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP

  tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
  tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

  tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
  tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
  */
  cfmakeraw(&cons_new_settings);
  cfsetspeed(&cons_new_settings, B115200);
  tcsetattr(tty, TCSANOW, &cons_new_settings);
}


void channel_teardown()
{
  close(tty);
  tty = -1;
}
