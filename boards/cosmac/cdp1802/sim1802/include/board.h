/****************************************************************************
 * boards/cosmac/cdp1802/sim1802/include/board.h
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

#ifndef __BOARDS_COSMAC_CDP1802_SIM1802_INCLUDE_BOARD_H
#define __BOARDS_COSMAC_CDP1802_SIM1802_INCLUDE_BOARD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Clocking: the nominal CPU clock; one machine cycle is 8 clock periods
 * [RCA MPM-201A p. 70].
 */

#define BOARD_CPU_CLOCK       CONFIG_BOARD_SIM1802_CLOCK_HZ
#define BOARD_MACHINE_CYCLE   (BOARD_CPU_CLOCK / 8)   /* cycles per second */

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifndef __ASSEMBLY__

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Name: cdp1802_boardinitialize
 *
 * Description:
 *   Board-specific initialization, called by the reset code before
 *   nx_start() (Step 06).
 *
 ****************************************************************************/

void cdp1802_boardinitialize(void);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __ASSEMBLY__ */
#endif /* __BOARDS_COSMAC_CDP1802_SIM1802_INCLUDE_BOARD_H */
