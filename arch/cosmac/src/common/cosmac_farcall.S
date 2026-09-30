/****************************************************************************
 * arch/cosmac/src/common/cosmac_farcall.S
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

/* Code banking: far call and far return (ADR 0013).
 *
 * Banked code runs in a 16 KiB window selected by a write-only latch on
 * output port CONFIG_COSMAC_BANK_PORT.  tools/cosmac/cosmac_bank.py gives
 * every banked function "foo" a stub in fixed ROM, under the original name:
 *
 *   foo:   sep  4                   ; NCRT call of the trampoline
 *          .hword __cosmac_farcall
 *          .hword __bank_foo        ; the banked function
 *          .byte  BANK
 *
 * so callers, function pointers and tail calls ("lbr foo") all enter the
 * stub exactly as they would enter foo.  __cosmac_farcall then
 *
 *   1. pushes {caller's return address, caller's bank} on the thread's bank
 *      return stack (BRS),
 *   2. replaces the return address on the C stack with __cosmac_farret, so
 *      the callee finds its arguments where the ABI puts them,
 *   3. selects the bank (RAM shadow, then the latch), and
 *   4. enters the callee with "sep 5": the NCRT return routine pops the
 *      callee's address, which the trampoline wrote over the stub's return
 *      address.
 *
 * __cosmac_farret, reached when the callee returns, pops the BRS entry,
 * re-selects the caller's bank and returns to the caller the same way.
 *
 * Both run with interrupts disabled between reading IE (LSIE) and the final
 * RET, so the BRS, the shadow and the latch always change together, and the
 * scratch bytes in g_cosmac_bankstate can be shared: nothing else uses them
 * while IE=0, and they are dead before IE is set again.  An interrupt
 * handler that calls banked code goes through the same stubs, and the
 * context switch saves and restores the first four bytes of the state per
 * thread.  Both routines keep R7..R15 and DF (the NCRT rules: only D and R6
 * may change) and keep R2 at or below the live stack top.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* g_cosmac_bankstate layout.  BRS entries are 3 bytes: return address (high
 * byte first) and the caller's bank; the BRS grows upwards.
 */

#define BS_BRSP      0   /* next free BRS entry (2 bytes, per thread) */
#define BS_FREE      2   /* free BRS entries (per thread) */
#define BS_BANK      3   /* selected bank: shadow of the latch (per thread) */
#define BS_FLAGS     4   /* scratch: IE << 1 | DF of the interrupted caller */
#define BS_R15       5   /* scratch: R15 (2 bytes) */
#define BS_SIZE      7

#define BRS_ENTRY    3

/****************************************************************************
 * Public Symbols
 ****************************************************************************/

	.global	__cosmac_farcall
	.global	__cosmac_farret
	.global	cosmac_bank_initialize
	.global	cosmac_bank_select
	.global	g_cosmac_bankstate
	.global	g_cosmac_idle_brs
	.global	cosmac_brs_overflow

/****************************************************************************
 * Name: __cosmac_farcall
 *
 * Description:
 *   Entered from a stub's NCRT call with P=3, X=2 and SP = E:
 *     M(E+1..E+2): address A of the stub's inline data {target, bank}
 *     M(E+3..E+4): return address into the stub's caller
 *   Leaves with SP = E+2, the callee's return address replaced with
 *   __cosmac_farret, the arguments unchanged.  When the callee's bank is
 *   the one selected, the return address is left alone and nothing is
 *   pushed on the bank return stack.
 *
 ****************************************************************************/

	.text
	.type	__cosmac_farcall, @function
__cosmac_farcall:

	/* D := IE, then IE := 0.  LSIE skips the nop and the lskp when IE=1,
	 * so "ldi 1" runs; when IE=0, lskp skips it.  An interrupt before the
	 * DIS returns with IE=1 again, so D stays right.
	 */

	ldi	0
	lsie
	nop
	lskp
	ldi	1
	sex	3
	dis				/* IE := 0; X,P := 2,3 */
	.byte	0x23
	shlc				/* D := IE << 1 | DF */
	str	2			/* M(E) := flags (the free byte) */

	/* Save the flags and R15 in the scratch bytes; R15 := &state */

	ldi	%hi(g_cosmac_bankstate + BS_FLAGS)
	phi	6
	ldi	%lo(g_cosmac_bankstate + BS_FLAGS)
	plo	6
	ldn	2
	str	6
	inc	6
	ghi	15
	str	6
	inc	6
	glo	15
	str	6
	ldi	%hi(g_cosmac_bankstate)
	phi	15
	ldi	%lo(g_cosmac_bankstate)
	plo	15

	/* R6 := A; copy the target over the stub's return address */

	inc	2			/* R2 = E+1 */
	lda	2
	phi	6
	ldn	2
	plo	6			/* R6 = A, R2 = E+2 */
	dec	2
	lda	6
	str	2			/* M(E+1) := target high */
	inc	2
	lda	6
	str	2			/* M(E+2) := target low */
	ldn	6
	plo	6			/* R6.0 := new bank */

	/* The callee's bank is already selected (a call through a function
	 * pointer or a stub from code of the same bank): no bank switch and no
	 * BRS entry.  Leave the stub's caller's return address in place, so
	 * that the callee returns to it directly.
	 */

	inc	15
	inc	15
	inc	15			/* R15 = &bank */
	sex	15
	glo	6
	xor				/* D := new bank ^ current bank */
	sex	2
	dec	15
	dec	15
	dec	15			/* R15 = &state */
	lbnz	2f
	dec	2
	dec	2			/* R2 = E */
	lbr	.Lenter

	/* One BRS entry less, or overflow */

2:

	inc	15
	inc	15			/* R15 = &free */
	ldn	15
	lbz	.Loverflow
	smi	1
	str	15

	/* R6.1 := old bank; select the new bank: shadow, then latch */

	inc	15			/* R15 = &bank */
	ldn	15
	phi	6
	glo	6
	str	15
	sex	15
	out	CONFIG_COSMAC_BANK_PORT	/* latch := M(R15); R15++ */
	sex	2

	/* Old bank into the free byte M(E); R6 := BRS pointer */

	dec	2
	dec	2			/* R2 = E */
	ghi	6
	str	2
	ldi	%lo(g_cosmac_bankstate + BS_BRSP)
	plo	15			/* R15 = &brsp (no page crossing) */
	lda	15
	phi	6
	ldn	15
	plo	6			/* R6 = BRS entry, R15 = &brsp + 1 */

	/* Entry := {return address, old bank}; the C stack's return address
	 * := __cosmac_farret.
	 */

	inc	2
	inc	2
	inc	2			/* R2 = E+3 */
	lda	2
	str	6
	inc	6
	ldn	2
	str	6
	inc	6			/* R2 = E+4 */
	ldi	%lo(__cosmac_farret)
	stxd
	ldi	%hi(__cosmac_farret)
	stxd				/* R2 = E+2 */
	dec	2
	dec	2			/* R2 = E */
	ldn	2
	str	6
	inc	6			/* R6 = next free entry */
	glo	6
	str	15
	dec	15
	ghi	6
	str	15			/* brsp := R6 */

	/* Restore R15 and DF; enter the callee with IE as it was */

.Lenter:

	ldi	%lo(g_cosmac_bankstate + BS_FLAGS)
	plo	15			/* R15 = &flags (no page crossing) */
	lda	15
	str	2			/* M(E) := flags */
	lda	15
	phi	6
	ldn	15
	plo	15
	ghi	6
	phi	15			/* R15 restored */
	ldn	2
	shr				/* DF restored, D := IE */
	lbz	1f
	sex	3
	ret				/* IE := 1; X,P := 2,3 */
	.byte	0x23
1:	sep	5			/* pops the target from M(E+1..E+2) */

.Loverflow:
	sep	4			/* does not return */
	.hword	cosmac_brs_overflow
	.size	__cosmac_farcall, .-__cosmac_farcall

/****************************************************************************
 * Name: __cosmac_farret
 *
 * Description:
 *   Reached through "sep 5" when a banked callee returns: P=3, X=2,
 *   SP = S (as before the far call), the return value in R7..R10.  Pops
 *   the BRS entry, re-selects the caller's bank and returns to the caller.
 *
 ****************************************************************************/

	.type	__cosmac_farret, @function
__cosmac_farret:
	ldi	0
	lsie
	nop
	lskp
	ldi	1
	sex	3
	dis
	.byte	0x23
	shlc
	dec	2
	dec	2
	str	2			/* M(S-2) := flags, R2 = S-2 */

	/* brsp -= 3; free += 1 */

	ldi	%hi(g_cosmac_bankstate + BS_BRSP + 1)
	phi	6
	ldi	%lo(g_cosmac_bankstate + BS_BRSP + 1)
	plo	6
	ldn	6
	smi	BRS_ENTRY
	str	6
	dec	6
	ldn	6
	smbi	0
	str	6			/* R6 = &brsp */
	inc	6
	inc	6
	ldn	6
	adi	1
	str	6
	dec	6
	dec	6

	/* R6 := the entry (via the free byte M(S-3)) */

	lda	6
	dec	2
	str	2			/* R2 = S-3 */
	ldn	6
	plo	6
	ldn	2
	phi	6

	/* Return address into M(S-1..S), where "sep 5" pops it */

	inc	2
	inc	2			/* R2 = S-1 */
	lda	6
	str	2
	inc	2
	lda	6
	str	2			/* R2 = S */
	ldn	6			/* D := the caller's bank */
	dec	2
	dec	2
	dec	2
	str	2			/* M(S-3) := bank, R2 = S-3 */

	/* Select it: shadow, then latch */

	ldi	%hi(g_cosmac_bankstate + BS_BANK)
	phi	6
	ldi	%lo(g_cosmac_bankstate + BS_BANK)
	plo	6
	ldn	2
	str	6
	sex	6
	out	CONFIG_COSMAC_BANK_PORT
	sex	2

	/* Restore DF; return with IE as it was */

	inc	2			/* R2 = S-2 */
	ldn	2
	shr
	lbz	1f
	sex	3
	ret
	.byte	0x23
1:	sep	5			/* pops the return address */
	.size	__cosmac_farret, .-__cosmac_farret

/****************************************************************************
 * Name: cosmac_bank_initialize
 *
 * Description:
 *   Called once from the reset code, before any banked function: the idle
 *   thread's BRS, bank 0 selected (the latch powers up undefined).
 *
 ****************************************************************************/

	.type	cosmac_bank_initialize, @function
cosmac_bank_initialize:
	ldi	%hi(g_cosmac_bankstate)
	phi	6
	ldi	%lo(g_cosmac_bankstate)
	plo	6
	ldi	%hi(g_cosmac_idle_brs)
	str	6
	inc	6
	ldi	%lo(g_cosmac_idle_brs)
	str	6
	inc	6
	ldi	CONFIG_COSMAC_BRS_DEPTH
	str	6
	inc	6
	ldi	0
	str	6
	sex	6
	out	CONFIG_COSMAC_BANK_PORT
	sex	2
	sep	5
	.size	cosmac_bank_initialize, .-cosmac_bank_initialize

/****************************************************************************
 * Name: cosmac_bank_select
 *
 * Description:
 *   void cosmac_bank_select(uint8_t bank): write the latch (the caller
 *   keeps the shadow in g_cosmac_bankstate).  Context switch only.
 *
 ****************************************************************************/

	.type	cosmac_bank_select, @function
cosmac_bank_select:
	glo	7
	str	2			/* the free byte M(SP) */
	out	CONFIG_COSMAC_BANK_PORT	/* R2++ */
	dec	2
	sep	5
	.size	cosmac_bank_select, .-cosmac_bank_select

/****************************************************************************
 * Data
 ****************************************************************************/

	/* The trampolines step R15 within the state with PLO only: it must not
	 * cross a 256-byte page, hence the alignment.
	 */

	.section .bss.g_cosmac_bankstate, "aw", @nobits
	.balign	8
	.type	g_cosmac_bankstate, @object
g_cosmac_bankstate:
	.skip	BS_SIZE
	.size	g_cosmac_bankstate, .-g_cosmac_bankstate

	.section .bss.g_cosmac_idle_brs, "aw", @nobits
	.type	g_cosmac_idle_brs, @object
g_cosmac_idle_brs:
	.skip	BRS_ENTRY * CONFIG_COSMAC_BRS_DEPTH
	.size	g_cosmac_idle_brs, .-g_cosmac_idle_brs
