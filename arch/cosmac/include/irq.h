/****************************************************************************
 * arch/cosmac/include/irq.h
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

/* This file should never be included directly but, rather, only indirectly
 * through nuttx/irq.h
 */

#ifndef __ARCH_COSMAC_INCLUDE_IRQ_H
#define __ARCH_COSMAC_INCLUDE_IRQ_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#ifndef __ASSEMBLY__
#  include <stdint.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* IRQ numbers.  The CDP1802 has a single INTERRUPT input; the sim1802
 * board's interrupt controller multiplexes eight sources onto it and
 * reports the source number on acknowledge.
 */

#define COSMAC_IRQ_TIMER      0   /* sim1802 timer */
#define COSMAC_IRQ_CONSOLE    6   /* sim1802 console input */
#define NR_IRQS               8

/* Register frame (docs/cdp1802/context.md in the port's meta repository).
 * The interrupt entry (cosmac_irqentry.S) pushes it on the interrupted
 * thread's stack, and a thread-level context switch builds the same frame.
 * Offsets are from the lowest address (the frame base); R2 is not stored:
 * the thread's stack pointer is the frame base + XCPTCONTEXT_SIZE.  R0
 * (DMA) and R1 (the interrupt PC) are not part of a thread's context.
 */

/* Rn (3..15): high byte at REG_R(n), low byte at REG_R(n) + 1 */

#define REG_BUF               0   /* sim1802 I/O argument buffer */
#define REG_R(n)              (1 + 2 * (15 - (n)))
#define REG_IE                27  /* IE to resume with (0 or 1) */
#define REG_DF                28  /* DF in bit 0 */
#define REG_D                 29  /* D accumulator */
#define REG_XP                30  /* T: X (high nibble), P (low nibble) */
#define XCPTCONTEXT_REGS      31
#define XCPTCONTEXT_SIZE      XCPTCONTEXT_REGS

#define REG_PCH               REG_R(3)
#define REG_PCL               (REG_R(3) + 1)

/* Stacks are byte aligned on the CDP1802 */

#define STACKFRAME_ALIGN      1

/****************************************************************************
 * Public Types
 ****************************************************************************/

#ifndef __ASSEMBLY__

struct xcptcontext
{
  /* The saved register frame, on the thread's own stack, while the thread
   * is not running.
   */

  FAR uint8_t *regs;

#ifdef CONFIG_COSMAC_BANKING
  /* Code banking (cosmac_farcall.S): the thread's copy of the first four
   * bytes of g_cosmac_bankstate (BRS pointer, free entries, bank) while it
   * is not running, and its bank return stack.
   */

  uint8_t bankstate[4];
  uint8_t brs[3 * CONFIG_COSMAC_BRS_DEPTH];
#endif
};

#endif /* __ASSEMBLY__ */

/****************************************************************************
 * Public Data
 ****************************************************************************/

#ifndef __ASSEMBLY__

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/* This holds a reference to the current interrupt level register storage
 * structure.  It is non-NULL only during interrupt processing.
 */

EXTERN volatile uint8_t *g_current_regs;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Interrupt enable state (docs/journal/step-02.md, Q4): read with LSIE,
 * changed with DIS/RET.  Implemented in cosmac_irqsave.S.
 */

irqstate_t up_irq_save(void);
void up_irq_restore(irqstate_t flags);
void up_irq_enable(void);

/****************************************************************************
 * Inline functions
 ****************************************************************************/

static inline_function FAR uint8_t *up_current_regs(void)
{
  return (FAR uint8_t *)g_current_regs;
}

static inline_function void up_set_current_regs(FAR uint8_t *regs)
{
  g_current_regs = regs;
}

/* Return true if we are currently executing in interrupt context */

#define up_interrupt_context() (up_current_regs() != NULL)

/* Return the current value of the stack pointer, R2 */

static inline_function uint16_t up_getsp(void)
{
  uint16_t sp;

  __asm__ __volatile__
    (
      "ghi 2\n\t"
      "phi %0\n\t"
      "glo 2\n\t"
      "plo %0"
      : "=r" (sp)
    );

  return sp;
}

/* Registers saved in a context */

#define up_getusrpc(regs) \
  ((((uint8_t *)((regs) ? (regs) : up_current_regs()))[REG_PCH] << 8) | \
    ((uint8_t *)((regs) ? (regs) : up_current_regs()))[REG_PCL])

#define up_getusrsp(regs) \
  ((uintptr_t)(regs) + XCPTCONTEXT_SIZE)

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __ASSEMBLY__ */
#endif /* __ARCH_COSMAC_INCLUDE_IRQ_H */
