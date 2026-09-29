/****************************************************************************
 * arch/cosmac/include/types.h
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
 * through nuttx/compiler.h or sys/types.h.
 *
 * CDP1802 C ABI (docs/cdp1802/abi.md in the port's meta repository):
 * char is 8 bits and UNSIGNED by default, short and int are 16 bits, long
 * is 32 bits, long long is 64 bits, pointers are 16 bits, all types are
 * byte aligned and big-endian.  The typedefs below match the compiler's own
 * __SIZE_TYPE__ definitions (cdp1802-elf-gcc -dM -E); like AVR, the 16-bit
 * types are int rather than GCC's short (same size and representation) so
 * that the PRI/SCN macros of inttypes.h ("d", "x") match them.
 */

#ifndef __ARCH_COSMAC_INCLUDE_TYPES_H
#define __ARCH_COSMAC_INCLUDE_TYPES_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Types
 ****************************************************************************/

#ifndef __ASSEMBLY__

/* These are the sizes of the standard integer types.  NOTE that these type
 * names have a leading underscore character.  This file will be included
 * (indirectly) by include/stdint.h and typedef'ed to the final name without
 * the underscore character.
 */

typedef signed char        _int8_t;    /* char is 8 bits (and unsigned) */
typedef unsigned char      _uint8_t;

typedef signed int         _int16_t;   /* short and int are 16 bits */
typedef unsigned int       _uint16_t;

typedef signed long        _int32_t;   /* long is 32 bits */
typedef unsigned long      _uint32_t;

typedef signed long long   _int64_t;   /* long long is 64 bits */
typedef unsigned long long _uint64_t;
#define __INT64_DEFINED

typedef _int64_t           _intmax_t;
typedef _uint64_t          _uintmax_t;

#if defined(__WCHAR_TYPE__)
typedef __WCHAR_TYPE__     _wchar_t;   /* 32 bits (long unsigned int) */
#else
typedef unsigned long      _wchar_t;
#endif

#if defined(__WINT_TYPE__)
typedef __WINT_TYPE__      _wint_t;
#else
typedef unsigned int       _wint_t;
#endif

typedef int                _wctype_t;

/* size_t is 16 bits (unsigned int) */

#if defined(__SIZE_TYPE__)
/* Derive ssize_t from size_t so that they differ only by their signedness */

#define unsigned signed
typedef __SIZE_TYPE__      _ssize_t;
#undef unsigned
typedef __SIZE_TYPE__      _size_t;
#else
typedef signed int         _ssize_t;
typedef unsigned int       _size_t;
#endif

/* The address space is 16 bits: there are no FAR pointers */

typedef signed int         _int_farptr_t;
typedef unsigned int       _uint_farptr_t;

/* This is the type of the interrupt state returned by up_irq_save():
 * the value of the IE flip-flop, 0 or 1.
 */

typedef unsigned char      irqstate_t;

#endif /* __ASSEMBLY__ */

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#endif /* __ARCH_COSMAC_INCLUDE_TYPES_H */
