/****************************************************************************
 * arch/cosmac/src/common/cosmac_initialstate.c
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
#include <string.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/sched.h>

#include "cosmac_internal.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* The return address of every thread's entry point, which never returns */

static void cosmac_thread_return(void)
{
  PANIC();
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_initial_state
 *
 * Description:
 *   A new thread is being started and a new TCB has been created.  This
 *   function is called to initialize the processor specific portions of
 *   the new TCB.
 *
 *   The idle thread already runs, on the stack set up by the reset code.
 *   Every other thread gets a register frame at the top of its stack, as
 *   if it had been interrupted just before its first instruction: P=3,
 *   X=2, IE=1, R3 = the entry point, R4/R5 = the NCRT routines.  Above the
 *   frame sits a return address, cosmac_thread_return(), for the entry
 *   point (which never returns); a banked entry point's stub needs it.
 *   Layout: docs/cdp1802/context.md in the port's meta repository.
 *
 ****************************************************************************/

void up_initial_state(FAR struct tcb_s *tcb)
{
  FAR struct xcptcontext *xcp = &tcb->xcp;
  uintptr_t ret = (uintptr_t)cosmac_thread_return;
  FAR uint8_t *top;
  FAR uint8_t *regs;

  memset(xcp, 0, sizeof(struct xcptcontext));

  if (tcb->pid == IDLE_PROCESS_ID)
    {
      FAR char *stack = (FAR char *)(g_idle_topstack -
                                     CONFIG_IDLETHREAD_STACKSIZE);

      tcb->stack_alloc_ptr = stack;
      tcb->stack_base_ptr  = stack;
      tcb->adj_stack_size  = CONFIG_IDLETHREAD_STACKSIZE;
      return;
    }

  top = (FAR uint8_t *)tcb->stack_base_ptr + tcb->adj_stack_size;
  *(top - 2) = (uint8_t)(ret >> 8);
  *(top - 1) = (uint8_t)ret;

  /* The thread's stack pointer is top - 3 (a free byte); the frame lies
   * just below it.
   */

  regs = top - 3 - XCPTCONTEXT_SIZE;
  memset(regs, 0, XCPTCONTEXT_SIZE);

  regs[REG_XP] = 0x23;
  regs[REG_IE] = 1;
  cosmac_setreg(regs, 3, (uintptr_t)tcb->start);
  cosmac_setreg(regs, 4, (uintptr_t)cosmac_ncrt_call);
  cosmac_setreg(regs, 5, (uintptr_t)cosmac_ncrt_ret);

  xcp->regs = regs;

#ifdef CONFIG_COSMAC_BANKING
  cosmac_bank_initstate(tcb);
#endif
}
