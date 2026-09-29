/****************************************************************************
 * boards/cosmac/cdp1802/sim1802/src/sim1802_ticktest.c
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

/* Step 07 exit test (CONFIG_BOARD_SIM1802_TICKTEST): replaces the idle
 * loop.  A watchdog re-arms itself every TICKTEST_PERIOD ticks; each
 * callback (interrupt context) records the tick count and the simulator's
 * machine-cycle counter.  Then a busy loop runs once with interrupts
 * disabled and once enabled; the difference, divided by the number of
 * ticks that occurred, is the cost of one timer interrupt (entry, dispatch,
 * nxsched_process_timer(), exit).  The unused part of the idle stack is
 * filled with a pattern at the start, so the report also shows how much of
 * it was used (by the idle loop and by the interrupts that arrived on it).
 * The results go to the console and the simulator halts with status 0.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdint.h>
#include <syslog.h>

#include <nuttx/arch.h>
#include <nuttx/clock.h>
#include <nuttx/irq.h>
#include <nuttx/wdog.h>

#include "cosmac_internal.h"
#include "sim1802_io.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TICKTEST_PERIOD   50      /* ticks between watchdog callbacks */
#define TICKTEST_COUNT    6       /* callbacks */
#define TICKTEST_LOOPS    4000    /* busy-loop iterations */
#define TICKTEST_COLOR    0x5a    /* unused idle stack */

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct wdog_s g_ticktest_wdog;
static volatile uint8_t g_ticktest_fired;
static clock_t g_ticktest_ticks[TICKTEST_COUNT + 1];
static uint32_t g_ticktest_cycles[TICKTEST_COUNT + 1];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void ticktest_callback(wdparm_t arg)
{
  uint8_t n = g_ticktest_fired + 1;

  g_ticktest_ticks[n]  = clock_systime_ticks();
  g_ticktest_cycles[n] = sim1802_cycles();
  g_ticktest_fired     = n;

  if (n < TICKTEST_COUNT)
    {
      wd_start(&g_ticktest_wdog, TICKTEST_PERIOD, ticktest_callback, 0);
    }
}

static uint32_t ticktest_busy(void)
{
  volatile unsigned int i;
  uint32_t start = sim1802_cycles();

  for (i = 0; i < TICKTEST_LOOPS; i++)
    {
    }

  return sim1802_cycles() - start;
}

static FAR uint8_t *ticktest_stack_bottom(void)
{
  return (FAR uint8_t *)(g_idle_topstack - CONFIG_IDLETHREAD_STACKSIZE);
}

static void ticktest_color(void)
{
  FAR uint8_t *p = ticktest_stack_bottom();
  FAR uint8_t *sp = (FAR uint8_t *)up_getsp();

  while (p < sp - 16)
    {
      *p++ = TICKTEST_COLOR;
    }
}

static unsigned int ticktest_stack_used(void)
{
  FAR uint8_t *p = ticktest_stack_bottom();

  while (*p == TICKTEST_COLOR)
    {
      p++;
    }

  return (unsigned int)((FAR uint8_t *)g_idle_topstack - p);
}

static void ticktest_report(void)
{
  irqstate_t flags;
  uint32_t quiet;
  uint32_t busy;
  clock_t ticks;
  int n;

  for (n = 1; n <= TICKTEST_COUNT; n++)
    {
      syslog(LOG_INFO, "ticktest: wdog %d: +%ld ticks, +%lu cycles\n", n,
             (long)(g_ticktest_ticks[n] - g_ticktest_ticks[n - 1]),
             (unsigned long)(g_ticktest_cycles[n] -
                             g_ticktest_cycles[n - 1]));
    }

  flags = up_irq_save();
  quiet = ticktest_busy();
  up_irq_restore(flags);

  ticks = clock_systime_ticks();
  busy  = ticktest_busy();
  ticks = clock_systime_ticks() - ticks;

  syslog(LOG_INFO, "ticktest: busy loop %lu cycles quiet, %lu cycles "
         "with %ld ticks\n", (unsigned long)quiet, (unsigned long)busy,
         (long)ticks);
  if (ticks > 0)
    {
      syslog(LOG_INFO, "ticktest: timer interrupt %lu cycles\n",
             (unsigned long)((busy - quiet) / ticks));
    }

  syslog(LOG_INFO, "ticktest: idle stack used %u of %u bytes\n",
         ticktest_stack_used(), CONFIG_IDLETHREAD_STACKSIZE);

  sim1802_halt(0);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_idle
 ****************************************************************************/

void up_idle(void)
{
  static bool started;

  if (!started)
    {
      started = true;
      ticktest_color();
      g_ticktest_ticks[0]  = clock_systime_ticks();
      g_ticktest_cycles[0] = sim1802_cycles();
      wd_start(&g_ticktest_wdog, TICKTEST_PERIOD, ticktest_callback, 0);
    }

  if (g_ticktest_fired >= TICKTEST_COUNT)
    {
      ticktest_report();
    }

  __asm__ __volatile__("idl");
}
