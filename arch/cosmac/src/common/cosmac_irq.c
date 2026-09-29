/****************************************************************************
 * arch/cosmac/src/common/cosmac_irq.c
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

#include "cosmac_internal.h"

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* Current interrupt context register save area (NULL: not in interrupt) */

volatile uint8_t *g_current_regs;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_irqinitialize
 *
 * Description:
 *   Called by nx_start() with interrupts disabled (IE=0 since reset): point
 *   R1 at the interrupt entry, quiet the board's interrupt controller, then
 *   enable interrupts.  Sources are enabled one by one by their drivers
 *   (up_enable_irq()).
 *
 ****************************************************************************/

void up_irqinitialize(void)
{
  g_current_regs = NULL;

  cosmac_irq_install();
  cosmac_irq_initialize();

#ifndef CONFIG_SUPPRESS_INTERRUPTS
  up_irq_enable();
#endif
}
