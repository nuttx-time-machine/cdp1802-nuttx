/****************************************************************************
 * boards/cosmac/cdp1802/sim1802/src/sim1802_lowputc.c
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
#include <nuttx/irq.h>

#include "sim1802_io.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_putc
 *
 * Description:
 *   Low-level console output: one byte to the simulator console.  The
 *   argument buffer is shared by all devices, so interrupts are disabled
 *   around the two OUTs.
 *
 ****************************************************************************/

void up_putc(int ch)
{
  irqstate_t flags = up_irq_save();

  sim1802_command(SIM1802_CMD_CONSOLE_PUTCHAR, (unsigned int)ch);
  up_irq_restore(flags);
}
