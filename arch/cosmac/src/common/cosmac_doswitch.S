/****************************************************************************
 * arch/cosmac/src/common/cosmac_doswitch.S
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

/* Thread-level context switch and full context restore (Step 08; frame:
 * arch/cosmac/include/irq.h, docs/cdp1802/context.md in the port's meta
 * repository).  Both end in cosmac_irq_exit, the restore path of the
 * interrupt handler, so there is one way to resume a thread.  That path
 * runs with P=1 (R1 as the PC), because it reloads R3: it is entered here
 * with interrupts disabled, which is what makes R1 free to use.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <arch/irq.h>

/****************************************************************************
 * Public Symbols
 ****************************************************************************/

	.global	cosmac_switchcontext
	.global	cosmac_fullcontextrestore
	.global	cosmac_sigreturn
	.global	cosmac_irq_exit
#ifdef CONFIG_COSMAC_IRQSTATE_SWFLAG
	.global	g_cosmac_ie
#endif

	.text

/****************************************************************************
 * Name: cosmac_switchcontext
 *
 * Description:
 *   void cosmac_switchcontext(uint8_t **saveregs, uint8_t *restoreregs)
 *
 *   Save the calling thread's context as a register frame on its own stack
 *   (*saveregs := the frame) and resume the frame at restoreregs.  The
 *   saved frame resumes at .Lresume, which returns from this call, with
 *   the interrupt state the caller had (normally disabled: the scheduler
 *   switches inside a critical section).  R7..R10 and R6, D and DF are
 *   not preserved (NCRT: caller-saved); R11..R15 are.
 *
 ****************************************************************************/

	.type	cosmac_switchcontext, @function
cosmac_switchcontext:

	/* R6.0 := IE, then IE := 0 */

#ifdef CONFIG_COSMAC_IRQSTATE_SWFLAG
	sex	3
	dis
	.byte	0x23
	ldi	%hi(g_cosmac_ie)
	phi	6
	ldi	%lo(g_cosmac_ie)
	plo	6
	ldn	6
#else
	ldi	0
	lsie				/* IE=1: skip the nop and the lskp */
	nop
	lskp				/* IE=0: skip the ldi 1 */
	ldi	1
	sex	3
	dis
	.byte	0x23			/* IE := 0; X,P := 2,3 */
#endif
	plo	6

	/* Push the frame exactly as the interrupt entry does, below the
	 * current stack pointer S (M(S) is left alone).
	 */

	dec	2
	ldi	0x23
	stxd				/* T: X=2, P=3 */
	stxd				/* D (not preserved) */
	stxd				/* DF (not preserved) */
	glo	6
	stxd				/* IE */
	ldi	%lo(.Lresume)
	stxd				/* R3 := .Lresume */
	ldi	%hi(.Lresume)
	stxd
	glo	4
	stxd
	ghi	4
	stxd
	glo	5
	stxd
	ghi	5
	stxd
	glo	6
	stxd
	ghi	6
	stxd
	glo	7
	stxd
	ghi	7
	stxd
	glo	8
	stxd
	ghi	8
	stxd
	glo	9
	stxd
	ghi	9
	stxd
	glo	10
	stxd
	ghi	10
	stxd
	glo	11
	stxd
	ghi	11
	stxd
	glo	12
	stxd
	ghi	12
	stxd
	glo	13
	stxd
	ghi	13
	stxd
	glo	14
	stxd
	ghi	14
	stxd
	glo	15
	stxd
	ghi	15
	stxd
	inp	6			/* I/O buffer */
	dec	2

	/* *saveregs := frame (R2 + 1), high byte first */

	glo	2
	plo	9
	ghi	2
	phi	9
	inc	9
	ghi	9
	str	7
	inc	7
	glo	9
	str	7

	/* Resume restoreregs (R8) */

	glo	8
	plo	7
	ghi	8
	phi	7
	lbr	cosmac_fullcontextrestore

	/* The saved frame resumes here, with SP = S and IE as it was */

.Lresume:
	sep	5			/* return from cosmac_switchcontext() */
	.size	cosmac_switchcontext, .-cosmac_switchcontext

/****************************************************************************
 * Name: cosmac_fullcontextrestore
 *
 * Description:
 *   void cosmac_fullcontextrestore(uint8_t *restoreregs)
 *
 *   Resume the register frame at restoreregs (R7).  Does not return.
 *
 ****************************************************************************/

	.type	cosmac_fullcontextrestore, @function
cosmac_fullcontextrestore:
	sex	3
	dis				/* IE := 0: R1 and R2 are ours now */
	.byte	0x23
	glo	7
	plo	2
	ghi	7
	phi	2			/* R2 := the frame */
	ldi	%hi(cosmac_irq_exit)
	phi	1
	ldi	%lo(cosmac_irq_exit)
	plo	1
	sep	1			/* cosmac_irq_exit, with P=1 */
	.size	cosmac_fullcontextrestore, .-cosmac_fullcontextrestore

/****************************************************************************
 * Name: cosmac_sigreturn
 *
 * Description:
 *   void cosmac_sigreturn(const uint8_t *copy, uint8_t *regs)
 *
 *   End of signal delivery: copy the saved frame (R7) back to its place on
 *   the stack (R8) and resume it.  The caller's own stack lies there, so
 *   interrupts are disabled first and the copy uses registers only.  Does
 *   not return.
 *
 ****************************************************************************/

	.type	cosmac_sigreturn, @function
cosmac_sigreturn:
	sex	3
	dis
	.byte	0x23
	ldi	XCPTCONTEXT_SIZE
	plo	9
1:	lda	7
	str	8
	inc	8
	dec	9
	glo	9
	lbnz	1b
	glo	8			/* R7 := regs (R8 - XCPTCONTEXT_SIZE) */
	smi	XCPTCONTEXT_SIZE
	plo	7
	ghi	8
	smbi	0
	phi	7
	lbr	cosmac_fullcontextrestore
	.size	cosmac_sigreturn, .-cosmac_sigreturn
