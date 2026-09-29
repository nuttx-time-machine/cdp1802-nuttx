/****************************************************************************
 * boards/cosmac/cdp1802/sim1802/src/sim1802_lowputc.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* sim1802 I/O controller (docs/cdp1802/board-sim1802.md, section 4):
 * OUT 6 writes the argument buffer, OUT 7 issues a command.
 */

#define SIM1802_CMD_CONSOLE_PUTCHAR 0xe0

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_putc
 *
 * Description:
 *   Low-level console output: one byte to the simulator console.  The
 *   argument buffer is shared by all devices, so the two OUTs must not be
 *   separated by an interrupt handler that uses the controller: interrupts
 *   are disabled around them.  OUT uses X=2 (the ABI invariant) and the free
 *   byte M(SP); it increments R2, which DEC 2 undoes.
 *
 ****************************************************************************/

void up_putc(int ch)
{
  irqstate_t flags = up_irq_save();

  __asm__ __volatile__
    (
      "glo %0\n\t"
      "str 2\n\t"
      "out 6\n\t"
      "dec 2\n\t"
      "ldi %1\n\t"
      "str 2\n\t"
      "out 7\n\t"
      "dec 2"
      :
      : "r" (ch), "i" (SIM1802_CMD_CONSOLE_PUTCHAR)
      : "memory"
    );

  up_irq_restore(flags);
}
