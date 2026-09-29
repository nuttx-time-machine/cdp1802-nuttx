/****************************************************************************
 * arch/cosmac/src/common/cosmac_doirq.c
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
#include <nuttx/board.h>

#include "sched/sched.h"
#include "cosmac_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: cosmac_doirq
 *
 * Description:
 *   Called by the interrupt entry (cosmac_irqentry.S) with interrupts
 *   disabled and the interrupted thread's register frame at regs.
 *   Acknowledges the interrupt at the board's controller, dispatches it,
 *   and returns the frame to resume: regs, or the frame of the thread that
 *   the handler made ready to run.
 *
 ****************************************************************************/

FAR uint8_t *cosmac_doirq(FAR uint8_t *regs)
{
  FAR struct tcb_s **running_task = &g_running_tasks[this_cpu()];
  FAR struct tcb_s *tcb = *running_task;
  int irq;

  tcb->xcp.regs = regs;
  up_set_current_regs(regs);

  irq = cosmac_irq_acknowledge();
  if (irq >= 0)
    {
      irq_dispatch(irq, regs);
      cosmac_irq_rearm(irq);
    }

  /* A handler may have made another thread ready to run */

  tcb = this_task();
  if (tcb != *running_task)
    {
      nxsched_switch_context(*running_task, tcb);
#ifdef CONFIG_COSMAC_BANKING
      cosmac_bank_switch(*running_task, tcb);
#endif
      *running_task = tcb;
    }

  up_set_current_regs(NULL);
  return tcb->xcp.regs;
}
