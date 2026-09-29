/****************************************************************************
 * boards/cosmac/cdp1802/sim1802/src/sim1802_io.h
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

#ifndef __BOARDS_COSMAC_CDP1802_SIM1802_SRC_SIM1802_IO_H
#define __BOARDS_COSMAC_CDP1802_SIM1802_SRC_SIM1802_IO_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* sim1802 I/O controller (sim1802_io.hrl of the simulator;
 * docs/cdp1802/board-sim1802.md, section 4): OUT 6 writes and INP 6 reads
 * the argument buffer, OUT 7 issues a command.
 */

#define SIM1802_CMD_HALT              0x00
#define SIM1802_CMD_IRQ_ACK           0x10  /* buffer := lowest pending and
                                             * enabled IRQ (8: none); its
                                             * pending and enable bits are
                                             * cleared */
#define SIM1802_CMD_IRQ_WRITE_ENABLED 0x13
#define SIM1802_CMD_IRQ_WRITE_PENDING 0x14
#define SIM1802_CMD_TIMER_CONTROL     0x80  /* 0: off, 4: every N cycles */
#define SIM1802_CMD_TIMER_PERIOD0     0x81  /* period bits 0..7 */
#define SIM1802_CMD_TIMER_PERIOD1     0x82  /* period bits 8..15 */
#define SIM1802_CMD_TIMER_PERIOD2     0x83  /* period bits 16..23 */
#define SIM1802_CMD_CYCLES_LATCH      0x84
#define SIM1802_CMD_CYCLES_READ0      0x85  /* latched count, byte 0..3 */
#define SIM1802_CMD_CONSOLE_PUTCHAR   0xe0

#define SIM1802_TIMER_CYCLES          4

/****************************************************************************
 * Inline Functions
 ****************************************************************************/

/* The argument buffer is shared by every device: call these with
 * interrupts disabled.  OUT uses X=2 (the ABI invariant) and the free byte
 * M(SP); it increments R2, which DEC 2 undoes.
 */

static inline void sim1802_command(unsigned int cmd, unsigned int arg)
{
  __asm__ __volatile__
    (
      "glo %1\n\t"
      "str 2\n\t"
      "out 6\n\t"
      "dec 2\n\t"
      "glo %0\n\t"
      "str 2\n\t"
      "out 7\n\t"
      "dec 2"
      :
      : "r" (cmd), "r" (arg)
      : "memory"
    );
}

static inline uint8_t sim1802_query(unsigned int cmd)
{
  unsigned int result;

  __asm__ __volatile__
    (
      "glo %1\n\t"
      "str 2\n\t"
      "out 7\n\t"
      "dec 2\n\t"
      "inp 6\n\t"
      "plo %0\n\t"
      "ldi 0\n\t"
      "phi %0"
      : "=&r" (result)
      : "r" (cmd)
      : "memory"
    );

  return (uint8_t)result;
}

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

uint32_t sim1802_cycles(void);
void sim1802_halt(int status) noreturn_function;

#endif /* __BOARDS_COSMAC_CDP1802_SIM1802_SRC_SIM1802_IO_H */
