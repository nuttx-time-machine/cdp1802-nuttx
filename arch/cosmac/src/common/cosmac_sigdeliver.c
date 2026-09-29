/****************************************************************************
 * arch/cosmac/src/common/cosmac_sigdeliver.c
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

#include <assert.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/sched.h>

#include "sched/sched.h"
#include "cosmac_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: cosmac_sigdeliver
 *
 * Description:
 *   The signal trampoline (see up_schedule_sigaction()): deliver the
 *   pending signals with interrupts enabled, then put the original frame
 *   back where it was and resume it.  This function runs on the stack
 *   just below that place, so cosmac_sigreturn() does the copy with
 *   interrupts disabled and without using the stack.
 *
 ****************************************************************************/

void cosmac_sigdeliver(void)
{
  FAR struct tcb_s *rtcb = this_task();

  DEBUGASSERT(rtcb->sigdeliver != NULL);

retry:
#ifndef CONFIG_SUPPRESS_INTERRUPTS
  up_irq_enable();
#endif

  (rtcb->sigdeliver)(rtcb);

#ifndef CONFIG_SUPPRESS_INTERRUPTS
  up_irq_save();
#endif

  if (!sq_empty(&rtcb->sigpendactionq) &&
      (rtcb->flags & TCB_FLAG_SIGNAL_ACTION) == 0)
    {
      goto retry;
    }

  rtcb->sigdeliver = NULL;
  rtcb->xcp.regs   = rtcb->xcp.saved_regs;
  cosmac_sigreturn(rtcb->xcp.sigregs, rtcb->xcp.saved_regs);
}
