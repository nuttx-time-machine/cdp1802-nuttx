/****************************************************************************
 * arch/cosmac/src/common/cosmac_internal.h
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

#ifndef __ARCH_COSMAC_SRC_COMMON_COSMAC_INTERNAL_H
#define __ARCH_COSMAC_SRC_COMMON_COSMAC_INTERNAL_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* First address above the idle stack (cdp1802_head.S); the heap starts
 * here and runs to the last RAM address, 0xFFFF, in every sim1802 memory
 * profile.
 */

extern const uintptr_t g_idle_topstack;

/* Linker script symbols */

extern uint8_t _sdata[];
extern uint8_t _edata[];
extern uint8_t _sbss[];
extern uint8_t _ebss[];

#define COSMAC_RAM_LAST       0xffff

#endif /* __ARCH_COSMAC_SRC_COMMON_COSMAC_INTERNAL_H */
