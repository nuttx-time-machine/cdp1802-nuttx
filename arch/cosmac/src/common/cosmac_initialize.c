/****************************************************************************
 * arch/cosmac/src/common/cosmac_initialize.c
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
#include <nuttx/board.h>

#include "cosmac_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_initialize
 *
 * Description:
 *   Called once by nx_start() after the basic OS services are up: register
 *   the board's serial devices, if any (the console is also available
 *   earlier through up_putc(), which needs no initialization).
 *
 ****************************************************************************/

void up_initialize(void)
{
#if defined(CONFIG_MCU_SERIAL) || defined(CONFIG_COSMAC_BOARD_CONSOLE)
  cosmac_serialinit();
#endif
}
