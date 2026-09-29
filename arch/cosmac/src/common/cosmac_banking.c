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

#include "cosmac_internal.h"

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
