#ifndef _SERIAL_COM_EXT_H_
#define _SERIAL_COM_EXT_H_

// THESE functions are platform specific to communcation mechanism and
// must be implemented elsewhere.
void write_byte(char byte);
char read_byte();
char find_byte_nonblocking(char target);
void channel_setup(char* linename);
void channel_teardown();

#endif // _SERIAL_COM_EXT_H_
