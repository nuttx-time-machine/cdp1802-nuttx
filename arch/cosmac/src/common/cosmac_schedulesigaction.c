/****************************************************************************
 * arch/cosmac/src/common/cosmac_schedulesigaction.c
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
#include <nuttx/sched.h>

#include "cosmac_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_schedule_sigaction
 *
 * Description:
 *   Arrange for tcb to run cosmac_sigdeliver() the next time it resumes.
 *   Called when tcb is not running, or when it is the running task and we
 *   are in an interrupt handler; in both cases tcb->xcp.regs is the frame
 *   it will resume.  The frame is redirected in place: its original
 *   contents are copied to the TCB, and it now enters cosmac_sigdeliver()
 *   with interrupts disabled, X=2, P=3 and the NCRT registers at their
 *   entry points.  (A copy of the frame below it, as other ports build,
 *   would overwrite the interrupt handler's own stack: there is no
 *   separate interrupt stack.)
 *
 *   The copy includes the byte above the frame, M(SP): cosmac_sigdeliver()
 *   starts with SP there and overwrites it, but a thread interrupted
 *   between "lda 2" and "ldn 2" in cosmac_ncrt_ret still has to read the
 *   low byte of its return address from it.  The interrupt entry keeps
 *   that byte for the same reason.
 *
 ****************************************************************************/

void up_schedule_sigaction(FAR struct tcb_s *tcb)
{
  FAR uint8_t *regs = tcb->xcp.regs;

  memcpy(tcb->xcp.sigregs, regs, XCPTCONTEXT_SIGSIZE);
  tcb->xcp.saved_regs = regs;

  regs[REG_XP] = 0x23;
  regs[REG_IE] = 0;
  cosmac_setreg(regs, 3, (uintptr_t)cosmac_sigdeliver);
  cosmac_setreg(regs, 4, (uintptr_t)cosmac_ncrt_call);
  cosmac_setreg(regs, 5, (uintptr_t)cosmac_ncrt_ret);
}
