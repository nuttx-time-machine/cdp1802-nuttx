/****************************************************************************
 * boards/cosmac/cdp1802/sim1802/src/sim1802_threadtest.c
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

/* Step 08 exit test (CONFIG_BOARD_SIM1802_THREADTEST): the init task's
 * entry point.  Every line it prints starts with "threads:".
 *
 * 1. Preemption: two SCHED_RR tasks of equal priority increment their own
 *    counter and never block; only time-slice expiry (an interrupt-level
 *    context switch) lets the other one run.  The init task samples both
 *    counters every 60 ms from a higher priority: they must take turns.
 * 2. Semaphores: two tasks pass the turn with two semaphores
 *    (thread-level context switches).
 * 3. usleep(): elapsed ticks.
 * 4. Signals: SIGUSR1 to a task blocked in sem_wait() (its saved frame is
 *    redirected to the signal trampoline; sem_wait() fails with EINTR),
 *    and SIGUSR2 from a watchdog (interrupt context) to the running task.
 *    Then a signal storm: a watchdog signals a running task 100 times
 *    while it makes near calls and checks their results, so that signals
 *    land on every kind of instruction, including the NCRT routines and
 *    code that keeps a temporary in the free byte M(R2).
 * 5. Context-switch cost: two tasks alternate with sched_yield().
 * All tasks return, so task exit is exercised too.  Then the simulator
 * halts with status 0.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <sched.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <syslog.h>
#include <unistd.h>

#include <nuttx/arch.h>
#include <nuttx/clock.h>
#include <nuttx/sched.h>
#include <nuttx/signal.h>
#include <nuttx/wdog.h>

#include "sim1802_io.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TT_PRIORITY      100
#define TT_STACKSIZE     1024
#define TT_RR_SAMPLES    12       /* 60 ms apart */
#define TT_SEM_ROUNDS    4
#define TT_YIELDS        200
#define TT_STORM         100      /* signals to a running task */

/****************************************************************************
 * Private Data
 ****************************************************************************/

static sem_t g_done;
static sem_t g_ready;
static sem_t g_ping;
static sem_t g_pong;
static sem_t g_never;
static struct wdog_s g_wdog;
static volatile bool g_stop;
static struct wdog_s g_stormdog;
static volatile unsigned int g_storm;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void tt_stack(FAR const char *name)
{
#ifdef CONFIG_STACK_COLORATION
  FAR struct tcb_s *tcb = nxsched_self();

  syslog(LOG_INFO, "threads: %s used %u of %u stack bytes\n", name,
         (unsigned int)up_check_tcbstack(tcb, 0),
         (unsigned int)tcb->adj_stack_size);
#endif
}

static pid_t tt_start(FAR const char *name, int priority, main_t entry,
                      FAR char * const *argv)
{
  pid_t pid = task_create(name, priority, TT_STACKSIZE, entry, argv);

  if (pid < 0)
    {
      syslog(LOG_ERR, "threads: FAIL task_create(%s): %d\n", name, errno);
    }

  return pid;
}

/* 1. Round-robin preemption: the tasks only count; the init task, at a
 * higher priority, samples the counters.
 */

static volatile uint16_t g_count[2];

static int tt_rr(int argc, FAR char *argv[])
{
  volatile uint16_t *count = &g_count[argv[1][0] - 'A'];

  while (!g_stop)
    {
      (*count)++;
    }

  sem_post(&g_done);
  return 0;
}

/* 2. Semaphore ping-pong */

static int tt_ping(int argc, FAR char *argv[])
{
  int i;

  for (i = 1; i <= TT_SEM_ROUNDS; i++)
    {
      sem_wait(&g_ping);
      syslog(LOG_INFO, "threads: sem ping %d\n", i);
      sem_post(&g_pong);
    }

  sem_post(&g_done);
  return 0;
}

static int tt_pong(int argc, FAR char *argv[])
{
  int i;

  for (i = 1; i <= TT_SEM_ROUNDS; i++)
    {
      sem_wait(&g_pong);
      syslog(LOG_INFO, "threads: sem pong %d\n", i);
      sem_post(&g_ping);
    }

  sem_post(&g_done);
  return 0;
}

/* 4. Signals */

static void tt_handler(int signo)
{
  syslog(LOG_INFO, "threads: signal %d handled\n", signo);
  if (signo == SIGUSR2)
    {
      g_stop = true;
    }
}

static void tt_catch(int signo)
{
  struct sigaction act;

  act.sa_handler = tt_handler;
  act.sa_flags   = 0;
  sigemptyset(&act.sa_mask);
  sigaction(signo, &act, NULL);
}

static int tt_sigblocked(int argc, FAR char *argv[])
{
  int ret;

  tt_catch(SIGUSR1);
  sem_post(&g_ready);

  ret = sem_wait(&g_never);
  syslog(LOG_INFO, "threads: blocked task: sem_wait() %s\n",
         ret < 0 && errno == EINTR ? "EINTR" : "FAIL");

  tt_stack("sigblocked");
  sem_post(&g_done);
  return 0;
}

static void tt_wdog(wdparm_t arg)
{
  nxsig_kill((pid_t)arg, SIGUSR2);
}

static int tt_sigrunning(int argc, FAR char *argv[])
{
  tt_catch(SIGUSR2);
  sem_post(&g_ready);

  while (!g_stop)
    {
    }

  syslog(LOG_INFO, "threads: running task stopped by its signal\n");
  tt_stack("sigrunning");
  sem_post(&g_done);
  return 0;
}

/* 4b. Signal storm */

static void tt_storm_handler(int signo)
{
  volatile unsigned int i;

  /* A varying amount of work, so that the next tick lands at another
   * point of the interrupted loop.
   */

  for (i = 0; i < g_storm % 7; i++)
    {
    }

  g_storm++;
}

static void tt_stormdog(wdparm_t arg)
{
  /* Every 7 to 9 ticks: delivering a signal takes several ticks, and a
   * signal that comes during a delivery is handled by that delivery, so
   * closer signals would rarely find the task in its loop.
   */

  nxsig_kill((pid_t)arg, SIGUSR1);
  if (g_storm < TT_STORM)
    {
      wd_start(&g_stormdog, 7 + g_storm % 3, tt_stormdog, arg);
    }
}

static unsigned int __attribute__((noipa)) tt_step(unsigned int x)
{
  return 3 * x + 1;
}

static int tt_storm_task(int argc, FAR char *argv[])
{
  struct sigaction act;
  unsigned int errors = 0;
  unsigned int calls = 0;
  unsigned int x = 1;
  unsigned int y;

  act.sa_handler = tt_storm_handler;
  act.sa_flags   = 0;
  sigemptyset(&act.sa_mask);
  sigaction(SIGUSR1, &act, NULL);
  sem_post(&g_ready);

  while (g_storm < TT_STORM)
    {
      y = tt_step(x);
      if (y != 3 * x + 1)
        {
          errors++;
        }

      x = y ^ calls++;
    }

  syslog(LOG_INFO, "threads: signal storm: %u signals, %u errors\n",
         g_storm, errors);
  sem_post(&g_done);
  return 0;
}

/* 5. Context switch cost */

static int tt_yield_partner(int argc, FAR char *argv[])
{
  while (!g_stop)
    {
      sched_yield();
    }

  sem_post(&g_done);
  return 0;
}

static int tt_yield(int argc, FAR char *argv[])
{
  uint32_t cycles;
  clock_t ticks;
  int i;

  ticks  = clock_systime_ticks();
  cycles = sim1802_cycles();
  for (i = 0; i < TT_YIELDS; i++)
    {
      sched_yield();
    }

  cycles = sim1802_cycles() - cycles;
  ticks  = clock_systime_ticks() - ticks;
  g_stop = true;

  syslog(LOG_INFO, "threads: %d sched_yield() round trips: %lu cycles, "
         "%ld ticks\n", TT_YIELDS, (unsigned long)cycles, (long)ticks);
  sem_post(&g_done);
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int sim1802_threadtest_main(int argc, FAR char *argv[])
{
  static char *const arg_a[] =
  {
    "A", NULL
  };

  static char *const arg_b[] =
  {
    "B", NULL
  };

  struct sched_param param;
  pid_t pid;
  int i;

  syslog(LOG_INFO, "threads: start\n");
  sem_init(&g_done, 0, 0);
  sem_init(&g_ready, 0, 0);
  sem_init(&g_ping, 0, 1);
  sem_init(&g_pong, 0, 0);
  sem_init(&g_never, 0, 0);

  /* 1. Preemption: counters sampled from a higher priority */

  param.sched_priority = TT_PRIORITY + 20;
  sched_setparam(0, &param);
  g_stop = false;
  tt_start("rrA", TT_PRIORITY, tt_rr, arg_a);
  tt_start("rrB", TT_PRIORITY, tt_rr, arg_b);
  for (i = 0; i < TT_RR_SAMPLES; i++)
    {
      usleep(60000);
      syslog(LOG_INFO, "threads: rr A=%u B=%u\n",
             (unsigned int)g_count[0], (unsigned int)g_count[1]);
    }

  g_stop = true;
  sem_wait(&g_done);
  sem_wait(&g_done);
  param.sched_priority = TT_PRIORITY;
  sched_setparam(0, &param);

  /* 2. Semaphores */

  tt_start("ping", TT_PRIORITY, tt_ping, NULL);
  tt_start("pong", TT_PRIORITY, tt_pong, NULL);
  sem_wait(&g_done);
  sem_wait(&g_done);

  /* 3. usleep() */

  for (i = 0; i < 3; i++)
    {
      clock_t start = clock_systime_ticks();

      usleep(200000);
      syslog(LOG_INFO, "threads: usleep(200 ms): %ld ticks\n",
             (long)(clock_systime_ticks() - start));
    }

  /* 4. Signals: to a blocked task, then from an interrupt */

  pid = tt_start("sigblocked", TT_PRIORITY, tt_sigblocked, NULL);
  sem_wait(&g_ready);
  usleep(100000);
  kill(pid, SIGUSR1);
  sem_wait(&g_done);

  g_stop = false;
  pid = tt_start("sigrunning", TT_PRIORITY, tt_sigrunning, NULL);
  sem_wait(&g_ready);
  wd_start(&g_wdog, 10, tt_wdog, (wdparm_t)pid);
  sem_wait(&g_done);

  g_storm = 0;
  pid = tt_start("storm", TT_PRIORITY, tt_storm_task, NULL);
  sem_wait(&g_ready);
  wd_start(&g_stormdog, 1, tt_stormdog, (wdparm_t)pid);
  sem_wait(&g_done);

  /* 5. Context switch cost: both tasks above the init task's priority,
   * created before either can run.
   */

  g_stop = false;
  sched_lock();
  tt_start("yield2", TT_PRIORITY + 10, tt_yield_partner, NULL);
  tt_start("yield1", TT_PRIORITY + 10, tt_yield, NULL);
  sched_unlock();
  sem_wait(&g_done);
  sem_wait(&g_done);

  tt_stack("init");
  syslog(LOG_INFO, "threads: done\n");
  sim1802_halt(0);
  return 0;
}
