/****************************************************************************
 * arch/cosmac/src/common/cosmac_banking.c
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

#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/sched.h>

#include "cosmac_internal.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* The bank return stack of a thread that is exiting (see
 * cosmac_bank_exitstate()).
 */

static uint8_t g_cosmac_exit_brs[3 * CONFIG_COSMAC_BRS_DEPTH];

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: cosmac_brs_overflow
 *
 * Description:
 *   Called by __cosmac_farcall, with interrupts disabled, when a thread
 *   nests more than CONFIG_COSMAC_BRS_DEPTH far calls.
 *
 ****************************************************************************/

void cosmac_brs_overflow(void)
{
  _alert("bank return stack overflow (CONFIG_COSMAC_BRS_DEPTH=%d)\n",
         CONFIG_COSMAC_BRS_DEPTH);
  PANIC();
}

/****************************************************************************
 * Name: cosmac_bank_initstate
 *
 * Description:
 *   A new thread: an empty bank return stack of its own and bank 0 (its
 *   entry point is in fixed ROM or a stub, which selects the bank itself).
 *
 ****************************************************************************/

void cosmac_bank_initstate(FAR struct tcb_s *tcb)
{
  uintptr_t brs = (uintptr_t)tcb->xcp.brs;

  tcb->xcp.bankstate[0] = (uint8_t)(brs >> 8);
  tcb->xcp.bankstate[1] = (uint8_t)brs;
  tcb->xcp.bankstate[2] = CONFIG_COSMAC_BRS_DEPTH;
  tcb->xcp.bankstate[3] = 0;
}

/****************************************************************************
 * Name: cosmac_bank_switch
 *
 * Description:
 *   Save the banking state of from (unless NULL) and make to's current,
 *   selecting its bank.  Called with interrupts disabled, from fixed code,
 *   just before to's register frame is resumed.  It must not call banked
 *   code, because the state it changes is the one the far-call trampoline
 *   uses: hence the loops instead of memcpy().
 *
 ****************************************************************************/

void cosmac_bank_switch(FAR struct tcb_s *from, FAR struct tcb_s *to)
{
  int i;

  if (from != NULL)
    {
      for (i = 0; i < 4; i++)
        {
          from->xcp.bankstate[i] = g_cosmac_bankstate[i];
        }
    }

  for (i = 0; i < 4; i++)
    {
      g_cosmac_bankstate[i] = to->xcp.bankstate[i];
    }

  cosmac_bank_select(g_cosmac_bankstate[3]);
}

/****************************************************************************
 * Name: cosmac_bank_exitstate
 *
 * Description:
 *   Called by up_exit() with interrupts disabled, before nxtask_exit()
 *   releases the exiting thread's TCB, and with it the bank return stack
 *   that the far calls it makes would use: switch to a static one, keeping
 *   the selected bank.  The thread's own entries are discarded, since it
 *   never returns.
 *
 ****************************************************************************/

void cosmac_bank_exitstate(void)
{
  uintptr_t brs = (uintptr_t)g_cosmac_exit_brs;

  g_cosmac_bankstate[0] = (uint8_t)(brs >> 8);
  g_cosmac_bankstate[1] = (uint8_t)brs;
  g_cosmac_bankstate[2] = CONFIG_COSMAC_BRS_DEPTH;
}
