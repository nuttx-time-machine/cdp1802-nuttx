/****************************************************************************
 * arch/cosmac/src/common/cosmac_stubs.c
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

/* STEP 04 STUBS: the architecture interfaces that NuttX common code needs
 * in order to link, each one PANIC()ing (or doing nothing, where doing
 * nothing is a correct minimal implementation).  They exist only for the
 * size-feasibility measurement; Steps 05-09 replace them with real code.
 * The list was produced by the linker: see docs/journal/step-04.md in the
 * meta repository.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <string.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/* Threads and context switching (Step 08) */

int up_create_stack(FAR struct tcb_s *tcb, size_t stack_size, uint8_t ttype)
{
  PANIC();
  return -1;
}

int up_use_stack(FAR struct tcb_s *tcb, FAR void *stack, size_t stack_size)
{
  PANIC();
  return -1;
}

void up_release_stack(FAR struct tcb_s *dtcb, uint8_t ttype)
{
  PANIC();
}

void up_switch_context(FAR struct tcb_s *tcb, FAR struct tcb_s *rtcb)
{
  PANIC();
}

/* up_saveusercontext() runs inside _assert(), so it must never PANIC():
 * until Step 08 defines the register frame it records a zeroed context.
 */

int up_saveusercontext(FAR void *saveregs)
{
  memset(saveregs, 0, XCPTCONTEXT_SIZE);
  return 0;
}

void up_exit(int status)
{
  PANIC();
  for (; ; );
}
