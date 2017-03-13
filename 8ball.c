#include "console.h"

int main (int argc, char **argv)
{
  console_setup(argc, argv);
  while(1){
    console();
  }
}
