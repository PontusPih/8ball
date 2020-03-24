 /*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#include <stdio.h>

#include "cpu.h"
#include "rx8.h"

// Controller bits and registers
short mode = 1; // 8-bit mode (0) or 12-bit mode (1)

void rx8_process(short mb)
{
  switch( mb & IOT_OP_MASK ){
  default:
    printf("illegal IOT instruction. Device: %o. Operation: %o - RX8\n", (mb & DEV_MASK) >> 3, mb & IOT_OP_MASK );
    exit(1);
    break;
  }
}
