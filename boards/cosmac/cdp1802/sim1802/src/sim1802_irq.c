/****************************************************************************
 * boards/cosmac/cdp1802/sim1802/src/sim1802_irq.c
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
#include <nuttx/irq.h>

#include "cosmac_internal.h"
#include "sim1802_io.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Sources enabled by drivers.  The controller clears a source's enable bit
 * when it acknowledges it, so the mask is written back after each handler.
 */

static uint8_t g_irqmask;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: cosmac_irq_initialize
 ****************************************************************************/

void cosmac_irq_initialize(void)
{
  g_irqmask = 0;
  sim1802_command(SIM1802_CMD_IRQ_WRITE_ENABLED, 0);
  sim1802_command(SIM1802_CMD_IRQ_WRITE_PENDING, 0);
}

/****************************************************************************
 * Name: cosmac_irq_acknowledge
 ****************************************************************************/

int cosmac_irq_acknowledge(void)
{
  uint8_t irq = sim1802_query(SIM1802_CMD_IRQ_ACK);

  return irq < NR_IRQS ? irq : -1;
}

/****************************************************************************
 * Name: cosmac_irq_rearm
 ****************************************************************************/

void cosmac_irq_rearm(int irq)
{
  sim1802_command(SIM1802_CMD_IRQ_WRITE_ENABLED, g_irqmask);
}

/****************************************************************************
 * Name: up_enable_irq
 ****************************************************************************/

void up_enable_irq(int irq)
{
  irqstate_t flags = up_irq_save();

  g_irqmask |= (uint8_t)(1 << irq);
  sim1802_command(SIM1802_CMD_IRQ_WRITE_ENABLED, g_irqmask);
  up_irq_restore(flags);
}

/****************************************************************************
 * Name: up_disable_irq
 ****************************************************************************/

void up_disable_irq(int irq)
{
  irqstate_t flags = up_irq_save();

  g_irqmask &= (uint8_t)~(1 << irq);
  sim1802_command(SIM1802_CMD_IRQ_WRITE_ENABLED, g_irqmask);
  up_irq_restore(flags);
}

/****************************************************************************
 * Name: sim1802_cycles
 *
 * Description:
 *   The simulator's machine-cycle counter (modulo 2^32), for measurements.
 *
 ****************************************************************************/

uint32_t sim1802_cycles(void)
{
  irqstate_t flags = up_irq_save();
  uint32_t cycles = 0;
  int i;

  sim1802_command(SIM1802_CMD_CYCLES_LATCH, 0);
  for (i = 3; i >= 0; i--)
    {
      cycles = (cycles << 8) | sim1802_query(SIM1802_CMD_CYCLES_READ0 + i);
    }

  up_irq_restore(flags);
  return cycles;
}

/****************************************************************************
 * Name: sim1802_halt
 *
 * Description:
 *   Stop the simulator; status becomes its exit status (tests only).
 *
 ****************************************************************************/

void sim1802_halt(int status)
{
  up_irq_save();
  sim1802_command(SIM1802_CMD_HALT, status);
  for (; ; );
}
