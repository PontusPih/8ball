/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

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
