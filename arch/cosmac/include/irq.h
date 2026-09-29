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

/* Register save area (PROVISIONAL: the byte-accurate layout is defined by
 * Step 08, docs/cdp1802/context.md).  Indices are byte offsets into
 * struct xcptcontext::regs[].
 */

#define REG_XP                0   /* T at interrupt time: X (high nibble), P */
#define REG_D                 1   /* D accumulator */
#define REG_DF                2   /* DF (bit 0) */
#define REG_IE                3   /* IE (bit 0) */
#define REG_R1H               4   /* R1 .. R15, high byte first */
#define REG_R(n)              (REG_R1H + 2 * ((n) - 1))
#define REG_SPH               REG_R(2)
#define REG_SPL               (REG_R(2) + 1)
#define REG_PCH               REG_R(3)
#define REG_PCL               (REG_R(3) + 1)
#define XCPTCONTEXT_REGS      (REG_R(15) + 2)
#define XCPTCONTEXT_SIZE      XCPTCONTEXT_REGS

/* Stacks are byte aligned on the CDP1802 */

#define STACKFRAME_ALIGN      1

/****************************************************************************
 * Public Types
 ****************************************************************************/

#ifndef __ASSEMBLY__

struct xcptcontext
{
  /* Register save area */

  uint8_t regs[XCPTCONTEXT_REGS];
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
  ((uintptr_t)((((uint8_t *)(regs))[REG_SPH] << 8) | \
                ((uint8_t *)(regs))[REG_SPL]))

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __ASSEMBLY__ */
#endif /* __ARCH_COSMAC_INCLUDE_IRQ_H */
