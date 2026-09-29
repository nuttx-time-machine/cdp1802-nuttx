/****************************************************************************
 * arch/cosmac/src/common/cosmac_ncrt.S
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

/* NCRT: the call and return routines of the CDP1802 C ABI
 * (docs/cdp1802/abi.md, section 4, in the port's meta repository).
 *
 * The 1802 has no call instruction.  Compiled code calls a function with
 *
 *      sep  4          ; P := 4: run the call routine, R4 points to it
 *      .hword callee   ; big-endian callee address, read by the routine
 *
 * and returns with "sep 5".  R4 and R5 are loaded once at reset with the
 * entry points below and never change (fixed registers of the ABI).
 *
 * Requirements (ABI): on entry to the callee X=2, the return address is on
 * the stack, high byte at the lower address, and SP points one byte below
 * it; D may be clobbered, DF must be preserved.  In addition (ADR 0006/0007)
 * R2 must be a valid stack pointer at every instruction boundary, because a
 * standard interrupt handler pushes through R2.
 *
 * Both routines end with the classic COSMAC trick of placing the final
 * "sep 3" immediately before the entry point: after it executes, R4 (R5) is
 * left pointing at the entry again, ready for the next call.
 *
 * Written from the ABI description and the subroutine techniques of RCA
 * MPM-201A (pp. 54-64); it does not derive from any other implementation.
 */

/****************************************************************************
 * Public Symbols
 ****************************************************************************/

	.global	cosmac_ncrt_call
	.global	cosmac_ncrt_ret

	.text

/****************************************************************************
 * Name: cosmac_ncrt_call
 *
 * Description:
 *   Entered with P=4 and R3 pointing at the inline callee address that
 *   follows the caller's "sep 4".  Pushes the return address (R3 + 2),
 *   loads the callee into R3 and continues there with P=3.
 *
 ****************************************************************************/

	.type	cosmac_ncrt_call, @function
cosmac_ncrt_call_exit:
	sep	3			/* enter the callee; R4 -> cosmac_ncrt_call */
cosmac_ncrt_call:
	sex	2			/* X=2 (ABI) */
	inc	3			/* R3 -> return address: INC keeps DF */
	inc	3
	glo	3
	stxd				/* push low byte of the return address */
	ghi	3
	stxd				/* push high byte: it ends up at the lower address */
	dec	3			/* R3 -> callee address, high byte */
	dec	3
	lda	3			/* D = callee high byte, R3 -> low byte */
	str	2			/* park it in the free byte M(SP) (ABI scratch) */
	ldn	3			/* D = callee low byte */
	plo	3
	ldn	2			/* D = callee high byte */
	phi	3			/* R3 = callee */
	lbr	cosmac_ncrt_call_exit
	.size	cosmac_ncrt_call, .-cosmac_ncrt_call

/****************************************************************************
 * Name: cosmac_ncrt_ret
 *
 * Description:
 *   Entered with P=5, SP one byte below the return address.  Pops it into
 *   R3 and continues there with P=3, SP back at its value before the call.
 *   An interrupt at any point finds R2 at or below the live stack top: the
 *   handler starts with "dec 2".
 *
 ****************************************************************************/

	.type	cosmac_ncrt_ret, @function
cosmac_ncrt_ret_exit:
	sep	3			/* return; R5 -> cosmac_ncrt_ret */
cosmac_ncrt_ret:
	sex	2			/* X=2 (ABI) */
	inc	2			/* SP -> high byte of the return address */
	lda	2			/* D = high byte, SP -> low byte */
	phi	3
	ldn	2			/* D = low byte; SP stays at the caller's value */
	plo	3
	lbr	cosmac_ncrt_ret_exit
	.size	cosmac_ncrt_ret, .-cosmac_ncrt_ret
