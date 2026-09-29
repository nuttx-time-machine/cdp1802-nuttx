/****************************************************************************
 * arch/cosmac/src/common/cosmac_registerdump.c
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
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static unsigned int reg16(FAR const uint8_t *regs, int n)
{
  return ((unsigned int)regs[REG_R(n)] << 8) | regs[REG_R(n) + 1];
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_dump_register
 *
 * Description:
 *   Dump a saved register context (struct xcptcontext::regs, layout in
 *   arch/cosmac/include/irq.h) after an assertion or crash.
 *
 ****************************************************************************/

void up_dump_register(FAR void *dumpregs)
{
  FAR const uint8_t *regs = dumpregs != NULL ?
                            (FAR const uint8_t *)dumpregs :
                            (FAR const uint8_t *)up_current_regs();
  int n;

  if (regs == NULL)
    {
      return;
    }

  _alert("XP:%02x D:%02x DF:%u IE:%u\n", regs[REG_XP], regs[REG_D],
         regs[REG_DF] & 1, regs[REG_IE] & 1);

  for (n = 1; n <= 15; n += 4)
    {
      _alert("R%-2d %04x %04x %04x %04x\n", n, reg16(regs, n),
             n + 1 <= 15 ? reg16(regs, n + 1) : 0,
             n + 2 <= 15 ? reg16(regs, n + 2) : 0,
             n + 3 <= 15 ? reg16(regs, n + 3) : 0);
    }
}
