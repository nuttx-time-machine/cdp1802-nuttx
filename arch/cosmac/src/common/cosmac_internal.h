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

#include <arch/irq.h>

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

/* Fill byte of unused stack memory (CONFIG_STACK_COLORATION) */

#define STACK_COLOR           0xa5

/* Store a 16-bit register value in a register frame (high byte first) */

#define cosmac_setreg(regs, n, value) \
  do \
    { \
      (regs)[REG_R(n)]     = (uint8_t)((uintptr_t)(value) >> 8); \
      (regs)[REG_R(n) + 1] = (uint8_t)(uintptr_t)(value); \
    } \
  while (0)

#ifdef CONFIG_COSMAC_BANKING
/* Code banking state (cosmac_farcall.S).  The first four bytes are per
 * thread: BRS pointer, free BRS entries, selected bank.
 */

extern uint8_t g_cosmac_bankstate[];
extern uint8_t g_cosmac_idle_brs[];
#endif

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef CONFIG_COSMAC_BANKING
void cosmac_brs_overflow(void) noreturn_function;
#endif

/* NCRT call and return routines (cosmac_ncrt.S) */

void cosmac_ncrt_call(void);
void cosmac_ncrt_ret(void);

/* Interrupts (cosmac_irqentry.S, cosmac_doirq.c) */

void cosmac_irq_install(void);
FAR uint8_t *cosmac_doirq(FAR uint8_t *regs);

/* Context switch and signals (cosmac_doswitch.S, cosmac_sigdeliver.c) */

void cosmac_switchcontext(FAR uint8_t **saveregs, FAR uint8_t *restoreregs);
void cosmac_fullcontextrestore(FAR uint8_t *restoreregs) noreturn_function;
void cosmac_sigdeliver(void);
void cosmac_sigreturn(FAR const uint8_t *copy, FAR uint8_t *regs)
  noreturn_function;

#ifdef CONFIG_COSMAC_BANKING
/* Per-thread banking state (cosmac_banking.c, cosmac_farcall.S) */

struct tcb_s;
void cosmac_bank_initstate(FAR struct tcb_s *tcb);
void cosmac_bank_switch(FAR struct tcb_s *from, FAR struct tcb_s *to);
void cosmac_bank_exitstate(void);
void cosmac_bank_select(uint8_t bank);
#endif

/* Provided by the board: its interrupt controller.  The CDP1802 has one
 * INTERRUPT input; the board multiplexes its sources onto it.
 *
 * cosmac_irq_initialize() - all sources disabled, nothing pending.
 * cosmac_irq_acknowledge() - called once per interrupt: the number of the
 *   source to dispatch (0 .. NR_IRQS - 1), or a negative value for a
 *   spurious interrupt.
 * cosmac_irq_rearm(irq) - called after the handler of irq has run, before
 *   the interrupt returns (e.g. to re-enable a source that the controller
 *   masks on acknowledge).
 */

void cosmac_irq_initialize(void);
int cosmac_irq_acknowledge(void);
void cosmac_irq_rearm(int irq);

/* Provided by the board when it has serial devices (CONFIG_MCU_SERIAL):
 * register them.  Called by up_initialize().
 */

#ifdef CONFIG_MCU_SERIAL
void cosmac_serialinit(void);
#endif

#endif /* __ARCH_COSMAC_SRC_COMMON_COSMAC_INTERNAL_H */
