/****************************************************************************
 * arch/cosmac/src/common/cosmac_irqsave.S
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

/* Interrupt enable state (docs/journal/step-02.md, Q4; ADR 0015 in the
 * port's meta repository).  COSMAC_IRQSTATE_LSIE (default): IE is read with
 * LSIE (long skip if IE=1).  COSMAC_IRQSTATE_SWFLAG: IE is read from its
 * RAM copy g_cosmac_ie, which these functions and the interrupt entry and
 * exit (cosmac_irqentry.S) keep equal to IE.  In both cases IE is changed
 * with DIS/RET.  DIS and RET load X,P from the byte at M(R(X)); with
 * SEX 3 that is the inline byte 0x23, which restores X=2, P=3, the state in
 * which all compiled code runs (docs/cdp1802/abi.md).  Tested in sim1802
 * (toolchain/tests/sim-audit/irqsave.S in the meta repository).
 *
 * Calling convention: NCRT, argument and result in R7 (low byte).
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Public Symbols
 ****************************************************************************/

	.global	up_irq_save
	.global	up_irq_restore
	.global	up_irq_enable
#ifdef CONFIG_COSMAC_IRQSTATE_SWFLAG
	.global	g_cosmac_ie
#endif

	.text

/****************************************************************************
 * Name: up_irq_save
 *
 * Description:
 *   Disable interrupts and return the previous IE value (0 or 1).
 *
 ****************************************************************************/

	.type	up_irq_save, @function
up_irq_save:
#ifdef CONFIG_COSMAC_IRQSTATE_SWFLAG
	sex	3
	dis				/* IE := 0 first: the copy is then stable */
	.byte	0x23			/* X=2, P=3 */
	ldi	%hi(g_cosmac_ie)
	phi	6
	ldi	%lo(g_cosmac_ie)
	plo	6
	ldn	6			/* IE before the DIS */
	plo	7
	ldi	0
	phi	7
	str	6			/* g_cosmac_ie := 0 */
	sep	5
#else
	ldi	0
	phi	7
	ldi	1			/* assume IE=1 */
	lsie				/* IE=1: skip the LDI 0 */
	ldi	0			/* IE was 0 */
	plo	7
	sex	3
	dis				/* IE := 0 */
	.byte	0x23			/* X=2, P=3 */
	sep	5
#endif
	.size	up_irq_save, .-up_irq_save

/****************************************************************************
 * Name: up_irq_restore
 *
 * Description:
 *   Restore the IE value returned by up_irq_save().
 *
 ****************************************************************************/

	.type	up_irq_restore, @function
up_irq_restore:
#ifdef CONFIG_COSMAC_IRQSTATE_SWFLAG
	sex	3
	dis				/* hold IE at 0 while the copy changes */
	.byte	0x23
	ldi	%hi(g_cosmac_ie)
	phi	6
	ldi	%lo(g_cosmac_ie)
	plo	6
	glo	7
	str	6
#endif
	glo	7
	lbz	1f			/* long branch: BZ only reaches its own page */
	sex	3
	ret				/* IE := 1 */
	.byte	0x23
	sep	5
1:	sex	3
	dis				/* IE := 0 */
	.byte	0x23
	sep	5
	.size	up_irq_restore, .-up_irq_restore

/****************************************************************************
 * Name: up_irq_enable
 *
 * Description:
 *   Enable interrupts unconditionally.
 *
 ****************************************************************************/

	.type	up_irq_enable, @function
up_irq_enable:
#ifdef CONFIG_COSMAC_IRQSTATE_SWFLAG
	sex	3
	dis				/* hold IE at 0 while the copy changes */
	.byte	0x23
	ldi	%hi(g_cosmac_ie)
	phi	6
	ldi	%lo(g_cosmac_ie)
	plo	6
	ldi	1
	str	6
#endif
	sex	3
	ret				/* IE := 1 */
	.byte	0x23
	sep	5
	.size	up_irq_enable, .-up_irq_enable

#ifdef CONFIG_COSMAC_IRQSTATE_SWFLAG
/****************************************************************************
 * Name: g_cosmac_ie
 *
 * Description:
 *   RAM copy of IE (0 or 1).  0 at reset: the reset code runs with IE=0.
 *
 ****************************************************************************/

	.section .bss.g_cosmac_ie, "aw", @nobits
	.type	g_cosmac_ie, @object
g_cosmac_ie:
	.skip	1
	.size	g_cosmac_ie, .-g_cosmac_ie
#endif
