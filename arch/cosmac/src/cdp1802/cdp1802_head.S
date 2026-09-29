/****************************************************************************
 * arch/cosmac/src/cdp1802/cdp1802_head.S
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

/* CDP1802 reset code (Step 06).
 *
 * Reset state [RCA MPM-201A p. 72]: P=0, X=0, R0=0x0000, IE=1; nothing else
 * is defined: RAM and the other registers hold arbitrary values (the
 * simulator's --rom mode makes them pseudo-random to catch mistakes).
 *
 * Register convention (ADR 0006): R1 = interrupt PC (set in Step 07),
 * R2 = stack pointer, R3 = program counter after reset, R4/R5 = NCRT call and
 * return routines, R0 never used after reset (it belongs to DMA).
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Public Symbols
 ****************************************************************************/

	.global	__start
	.global	g_idle_topstack
	.global	cosmac_ncrt_call
	.global	cosmac_ncrt_ret
	.global	cdp1802_boardinitialize
	.global	nx_start
#ifdef CONFIG_COSMAC_BANKING
	.global	cosmac_bank_initialize
#endif

/****************************************************************************
 * Reset entry point (linked first, at 0x0000)
 ****************************************************************************/

	.section .reset, "ax"
	.type	__start, @function
__start:
	dis				/* IE := 0; X,P := M(R0) = 0x00 */
	.byte	0x00

	/* The idle thread's stack is the CONFIG_IDLETHREAD_STACKSIZE bytes
	 * after .bss.  SP points one byte below the last pushed byte, so an
	 * empty stack has SP = top - 1.  (Composite R2 write with IE=0: exempt
	 * from the interrupt-safety rule, scripts/check-sp-safety.py --allow.)
	 */

	ldi	%hi(_ebss + CONFIG_IDLETHREAD_STACKSIZE - 1)
	phi	2
	ldi	%lo(_ebss + CONFIG_IDLETHREAD_STACKSIZE - 1)
	plo	2

	ldi	%hi(__start_c)
	phi	3
	ldi	%lo(__start_c)
	plo	3
	sep	3			/* PC := R3; R0 is never used again */
	.size	__start, .-__start

/****************************************************************************
 * Name: __start_c
 *
 * Description:
 *   Copy .data from ROM, clear .bss, set up the NCRT registers, then call
 *   the board initialization and nx_start() (which does not return).
 *   Runs with IE=0, P=3; R7..R10 are free (no C code has run yet).
 *
 ****************************************************************************/

	.text
	.type	__start_c, @function
__start_c:
	sex	2

	/* Copy .data: R8 = source in ROM (_eronly), R9 = _sdata, R10 = _edata */

	ldi	%hi(_eronly)
	phi	8
	ldi	%lo(_eronly)
	plo	8
	ldi	%hi(_sdata)
	phi	9
	ldi	%lo(_sdata)
	plo	9
	ldi	%hi(_edata)
	phi	10
	ldi	%lo(_edata)
	plo	10
1:	glo	9			/* while (R9 != R10) */
	str	2
	glo	10
	xor
	lbnz	2f
	ghi	9
	str	2
	ghi	10
	xor
	lbz	3f
2:	lda	8
	str	9
	inc	9
	lbr	1b

	/* Clear .bss: R9 = _sbss, R10 = _ebss */

3:	ldi	%hi(_sbss)
	phi	9
	ldi	%lo(_sbss)
	plo	9
	ldi	%hi(_ebss)
	phi	10
	ldi	%lo(_ebss)
	plo	10
4:	glo	9
	str	2
	glo	10
	xor
	lbnz	5f
	ghi	9
	str	2
	ghi	10
	xor
	lbz	6f
5:	ldi	0
	str	9
	inc	9
	lbr	4b

	/* NCRT: R4 = call routine, R5 = return routine */

6:	ldi	%hi(cosmac_ncrt_call)
	phi	4
	ldi	%lo(cosmac_ncrt_call)
	plo	4
	ldi	%hi(cosmac_ncrt_ret)
	phi	5
	ldi	%lo(cosmac_ncrt_ret)
	plo	5

#ifdef CONFIG_COSMAC_BANKING
	/* Bank state and latch, before any banked code (cosmac_farcall.S) */

	sep	4
	.hword	cosmac_bank_initialize
#endif

	/* Board hook, then the OS */

	sep	4
	.hword	cdp1802_boardinitialize
	sep	4
	.hword	nx_start

7:	lbr	7b			/* nx_start() does not return */
	.size	__start_c, .-__start_c

/****************************************************************************
 * Name: g_idle_topstack
 *
 * Description:
 *   The first address above the idle thread's stack, which is also the
 *   start of the heap (cosmac_allocateheap.c, cosmac_initialstate.c).
 *
 ****************************************************************************/

	.section .rodata
	.type	g_idle_topstack, @object
g_idle_topstack:
	.hword	_ebss + CONFIG_IDLETHREAD_STACKSIZE
	.size	g_idle_topstack, .-g_idle_topstack
