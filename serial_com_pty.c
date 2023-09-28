#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#define _BSD_SOURCE 1
#define __USE_MISC 1
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "serial_com_ext.h"

int pty = -1; // PTY handle

// "blocking" write of single byte
void write_byte(char byte)
{
  ssize_t rlen = write(pty, &byte, 1);

  if( rlen < 1 ){
    printf("Unable to write to PTY\n");
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
  int flags = fcntl(pty, F_GETFL, 0);
  fcntl(pty, F_SETFL, flags | O_NONBLOCK);
  do {
    len = read(pty, &byte, 1);
  } while ( len == 1 && byte != target );
  fcntl(pty, F_SETFL, flags);

  return len == 1 && byte == target;
}


// "blocking" read of one byte
char read_byte()
{
  char byte;
  ssize_t len = 0;
  do {
    len = read(pty, &byte, 1);
  } while( len == 0 );

  if( len < 0 ){
    printf("Unable to read from PTY\n");
    perror(__func__);
    exit(EXIT_FAILURE);
  }
  return byte;
}


void channel_setup(char* pty_name)
{
  if( pty_name == NULL ){ // No name? Create pty master
    if( (pty = posix_openpt(O_RDWR|O_NOCTTY)) == -1){
      printf("Unable to open PTMX\n");
      exit(EXIT_FAILURE);
    }

    if( grantpt(pty) == -1 ){
      printf("grantp() failed\n");
      exit(EXIT_FAILURE);
    }

    if( unlockpt(pty) == -1 ){
      printf("Unable to unlock PTY\n");
      exit(EXIT_FAILURE);
    }

    int fd = creat("ptyname.txt", S_IRUSR | S_IWUSR);
    write(fd, ptsname(pty), strlen(ptsname(pty)));
    close(fd);
  } else { // Name given, open as pty slave
    pty = open(pty_name, O_RDWR|O_NOCTTY);

    if( pty == -1 ){
      printf("Unable to open %s\n", pty_name);
      exit(EXIT_FAILURE);
    }

    struct termios cons_old_settings;
    struct termios cons_new_settings;
    tcgetattr(pty, &cons_old_settings);
    cons_new_settings = cons_old_settings;
    cfmakeraw(&cons_new_settings);
    tcsetattr(pty, TCSANOW, &cons_new_settings);
  }
}


void channel_teardown()
{
  close(pty);
  pty = -1;
}
