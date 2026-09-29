/****************************************************************************
 * arch/cosmac/src/common/cosmac_switchcontext.c
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
#include <nuttx/sched.h>

#include "sched/sched.h"
#include "cosmac_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_switch_context
 *
 * Description:
 *   A task is currently in the ready-to-run list but has been prepped to
 *   execute.  Restore its context, and start execution.
 *
 * Input Parameters:
 *   tcb: Refers to the head task of the ready-to-run list which will be
 *     executed.
 *   rtcb: Refers to the running task which will be blocked.
 *
 ****************************************************************************/

void up_switch_context(FAR struct tcb_s *tcb, FAR struct tcb_s *rtcb)
{
  /* In an interrupt handler, cosmac_doirq() notices the new head of the
   * ready-to-run list and resumes it when the handler returns.
   */

  if (!up_interrupt_context())
    {
      nxsched_switch_context(rtcb, tcb);
      g_running_tasks[this_cpu()] = tcb;

#ifdef CONFIG_COSMAC_BANKING
      cosmac_bank_switch(rtcb, tcb);
#endif

      /* Returns when rtcb runs again */

      cosmac_switchcontext(&rtcb->xcp.regs, tcb->xcp.regs);
    }
}
