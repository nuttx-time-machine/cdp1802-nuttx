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

/* CDP1802 reset code.
 *
 * STEP 04 STUB: this is only enough to link the kernel for the size
 * measurements.  It performs the documented reset sequence and calls
 * nx_start(), but does not yet set up the stack, copy .data, clear .bss or
 * provide the NCRT call/return routines that R4/R5 must point to (Step 06).
 *
 * Reset state [RCA MPM-201A p. 72]: P=0, X=0, R0=0x0000, IE=1.
 * Register convention (ADR 0006): R3 becomes the PC, R0 is never used
 * again (it belongs to DMA).
 */

/****************************************************************************
 * Public Symbols
 ****************************************************************************/

	.global	__start
	.global	nx_start

/****************************************************************************
 * Reset entry point (linked first, at 0x0000)
 ****************************************************************************/

	.section .reset, "ax"
	.type	__start, @function
__start:
	dis				/* IE := 0; X,P := M(R0) = 0x00 */
	.byte	0x00
	ldi	%hi(__start_c)
	phi	3
	ldi	%lo(__start_c)
	plo	3
	sep	3			/* PC := R3 */
	.size	__start, .-__start

	.text
	.type	__start_c, @function
__start_c:
	sep	4			/* NCRT call (Step 06: R4 not set up yet) */
	.hword	nx_start
1:	lbr	1b
	.size	__start_c, .-__start_c
