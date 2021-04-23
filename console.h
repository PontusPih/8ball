/*
  Copyright (c) 2019 Pontus Pihlgren <pontus.pihlgren@gmail.com>
  All rights reserved.

  This source code is licensed under the BSD-style license found in the
  LICENSE file in the root directory of this source tree.
*/

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

void console_setup(int argc, char **argv);
void console_stop_at(void);
void console_trace_instruction(void);

void console(void);

#endif // _CONSOLE_H_
