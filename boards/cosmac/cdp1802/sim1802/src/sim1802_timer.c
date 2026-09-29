/****************************************************************************
 * boards/cosmac/cdp1802/sim1802/src/sim1802_timer.c
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

#include <stdint.h>

#include <nuttx/arch.h>
#include <nuttx/clock.h>
#include <nuttx/irq.h>

#include <arch/board/board.h>

#include "sim1802_io.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Machine cycles per tick: 5 000 for a 4 MHz clock and 10 ms ticks.  The
 * simulator's timer counts machine cycles (24 bits).
 */

#define SIM1802_TICK_CYCLES \
  ((uint32_t)((unsigned long long)BOARD_MACHINE_CYCLE * \
              CONFIG_USEC_PER_TICK / 1000000))

#if (BOARD_MACHINE_CYCLE * CONFIG_USEC_PER_TICK / 1000000) < 1 || \
    (BOARD_MACHINE_CYCLE * CONFIG_USEC_PER_TICK / 1000000) > 0xffffff
#  error "CONFIG_USEC_PER_TICK out of range of the sim1802 timer"
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int sim1802_timerisr(int irq, FAR void *context, FAR void *arg)
{
  nxsched_process_timer();
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_timer_initialize
 *
 * Description:
 *   The system tick: the simulator's cycle-driven timer (mode 4) raises
 *   IRQ 0 every SIM1802_TICK_CYCLES machine cycles, so simulated time is
 *   exact and independent of the host.
 *
 ****************************************************************************/

void up_timer_initialize(void)
{
  uint32_t period = SIM1802_TICK_CYCLES;
  irqstate_t flags;

  irq_attach(COSMAC_IRQ_TIMER, sim1802_timerisr, NULL);

  flags = up_irq_save();
  sim1802_command(SIM1802_CMD_TIMER_PERIOD0, period & 0xff);
  sim1802_command(SIM1802_CMD_TIMER_PERIOD1, (period >> 8) & 0xff);
  sim1802_command(SIM1802_CMD_TIMER_PERIOD2, (period >> 16) & 0xff);
  sim1802_command(SIM1802_CMD_TIMER_CONTROL, SIM1802_TIMER_CYCLES);
  up_irq_restore(flags);

  up_enable_irq(COSMAC_IRQ_TIMER);
}
