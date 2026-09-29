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

#include <string.h>

#include <nuttx/arch.h>
#include <nuttx/sched.h>

#include "cosmac_internal.h"

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
 *   Step 06: only the idle thread, which already runs on the stack set up by
 *   the reset code.  The initial register frame of other threads is
 *   defined in Step 08 (docs/cdp1802/context.md).
 *
 ****************************************************************************/

void up_initial_state(FAR struct tcb_s *tcb)
{
  FAR struct xcptcontext *xcp = &tcb->xcp;

  if (tcb->pid == IDLE_PROCESS_ID)
    {
      FAR char *stack = (FAR char *)(g_idle_topstack -
                                     CONFIG_IDLETHREAD_STACKSIZE);

      tcb->stack_alloc_ptr = stack;
      tcb->stack_base_ptr  = stack;
      tcb->adj_stack_size  = CONFIG_IDLETHREAD_STACKSIZE;
    }
  else
    {
      PANIC();              /* Step 08 */
    }

  memset(xcp, 0, sizeof(struct xcptcontext));
}
