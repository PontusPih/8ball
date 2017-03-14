#include "console.h"
#include "machine.h"
#define UNUSED(x) (void)(x);

int main (int argc, char **argv)
{
#ifdef PTY_SRV
  UNUSED(argc);
  UNUSED(argv);
  machine_srv();
#else
  console_setup(argc, argv);
  console();
#endif
}
