/****************************************************************************
 * arch/cosmac/src/common/cosmac_irqentry.S
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

/* Interrupt entry and exit (Step 07; register frame: arch/cosmac/include/
 * irq.h, docs/cdp1802/context.md in the port's meta repository).
 *
 * On an interrupt the CPU does T := X,P; P := 1; X := 2; IE := 0
 * [RCA MPM-201A p. 40]: R1 is the interrupt PC and R2 the stack of the
 * interrupted thread (register convention, ADR 0006).  The entry pushes a
 * complete register frame below the interrupted stack pointer E, leaving
 * M(E) (the interrupted code's free byte) alone, then calls
 * cosmac_doirq(frame) in C.  cosmac_doirq() returns the frame to resume --
 * the same one, or another thread's after a context switch -- and the exit
 * path restores it: every register from R3 up, D, DF, the I/O argument
 * buffer, and finally X,P from T with RET (IE := 1) or DIS (IE := 0), as
 * recorded in the frame.  Popping the frame leaves R2 = E of that thread.
 *
 * R4/R5 are reloaded before the C call: the interrupt may have arrived
 * while P=4 or P=5 (inside the NCRT call or return routine), when they
 * point into the middle of it.  Their interrupted values are in the frame.
 *
 * The exit code ends with RET immediately before the entry, so after RET
 * R1 points at the entry again.  A frame resumed with DIS (a thread that
 * switched with interrupts disabled) leaves R1 at the long branch after
 * the DIS, which leads to the entry when the next interrupt comes.
 *
 * Interrupts stay disabled for the whole handler (no nesting).
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <arch/irq.h>

/****************************************************************************
 * Public Symbols
 ****************************************************************************/

	.global	cosmac_irq_entry
	.global	cosmac_irq_exit
	.global	cosmac_irq_install
	.global	cosmac_doirq
	.global	cosmac_ncrt_call
	.global	cosmac_ncrt_ret
#ifdef CONFIG_COSMAC_IRQSTATE_SWFLAG
	.global	g_cosmac_ie
#endif

	.text

/****************************************************************************
 * Name: cosmac_irq_exit
 *
 * Description:
 *   Resume the thread whose register frame starts at R2.  Entered with
 *   P=1, X=2 and IE=0, from the interrupt entry below or from a
 *   thread-level context switch.
 *
 ****************************************************************************/

	.type	cosmac_irq_exit, @function
cosmac_irq_exit:
#ifdef CONFIG_COSMAC_IRQSTATE_SWFLAG
	/* g_cosmac_ie := the IE this frame resumes with */

	glo	2
	adi	REG_IE
	plo	7
	ghi	2
	adci	0
	phi	7
	ldi	%hi(g_cosmac_ie)
	phi	6
	ldi	%lo(g_cosmac_ie)
	plo	6
	ldn	7
	str	6
#endif
	out	6			/* I/O buffer; R2 -> R15 */
	lda	2
	phi	15
	lda	2
	plo	15
	lda	2
	phi	14
	lda	2
	plo	14
	lda	2
	phi	13
	lda	2
	plo	13
	lda	2
	phi	12
	lda	2
	plo	12
	lda	2
	phi	11
	lda	2
	plo	11
	lda	2
	phi	10
	lda	2
	plo	10
	lda	2
	phi	9
	lda	2
	plo	9
	lda	2
	phi	8
	lda	2
	plo	8
	lda	2
	phi	7
	lda	2
	plo	7
	lda	2
	phi	6
	lda	2
	plo	6
	lda	2
	phi	5
	lda	2
	plo	5
	lda	2
	phi	4
	lda	2
	plo	4
	lda	2
	phi	3
	lda	2
	plo	3
	lda	2			/* IE */
	lbz	.Lresume_dis
	lda	2
	shr				/* DF */
	lda	2			/* D */
	ret				/* X,P := T; R2 := E; IE := 1 */
	.size	cosmac_irq_exit, .-cosmac_irq_exit

/****************************************************************************
 * Name: cosmac_irq_entry
 *
 * Description:
 *   The interrupt handler (R1 points here).  P=1, X=2, IE=0, R2 = E.
 *
 ****************************************************************************/

	.type	cosmac_irq_entry, @function
cosmac_irq_entry:
	dec	2			/* keep M(E), the thread's free byte */
	sav				/* T (X,P of the thread) */
	dec	2
	stxd				/* D */
	shlc
	stxd				/* DF (bit 0) */
	ldi	1
	stxd				/* IE: it was 1, or we would not be here */
	glo	3
	stxd
	ghi	3
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
	inp	6			/* I/O buffer -> M(R2) */
	dec	2			/* R2: free byte below the frame */

#ifdef CONFIG_COSMAC_IRQSTATE_SWFLAG
	ldi	%hi(g_cosmac_ie)	/* IE is 0 now */
	phi	6
	ldi	%lo(g_cosmac_ie)
	plo	6
	ldi	0
	str	6
#endif

	/* NCRT registers for the C code */

	ldi	%hi(cosmac_ncrt_call)
	phi	4
	ldi	%lo(cosmac_ncrt_call)
	plo	4
	ldi	%hi(cosmac_ncrt_ret)
	phi	5
	ldi	%lo(cosmac_ncrt_ret)
	plo	5

	/* R7 := frame; call cosmac_doirq(frame) with R3 as the PC */

	glo	2
	plo	7
	ghi	2
	phi	7
	inc	7
	ldi	%hi(.Lcall)
	phi	3
	ldi	%lo(.Lcall)
	plo	3
	sep	3

	/* R7 = the frame to resume (IE=0: R2 may be replaced byte by byte) */

	glo	7
	plo	2
	ghi	7
	phi	2
	lbr	cosmac_irq_exit

.Lcall:
	sep	4
	.hword	cosmac_doirq
	sep	1			/* back to the entry code above */

.Lresume_dis:
	lda	2
	shr				/* DF */
	lda	2			/* D */
	dis				/* X,P := T; R2 := E; IE := 0 */
	lbr	cosmac_irq_entry	/* R1 stays here until the next interrupt */
	.size	cosmac_irq_entry, .-cosmac_irq_entry

/****************************************************************************
 * Name: cosmac_irq_install
 *
 * Description:
 *   R1 := cosmac_irq_entry.  Called from up_irqinitialize() with
 *   interrupts disabled.
 *
 ****************************************************************************/

	.type	cosmac_irq_install, @function
cosmac_irq_install:
	ldi	%hi(cosmac_irq_entry)
	phi	1
	ldi	%lo(cosmac_irq_entry)
	plo	1
	sep	5
	.size	cosmac_irq_install, .-cosmac_irq_install
