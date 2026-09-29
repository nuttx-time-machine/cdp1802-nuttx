/****************************************************************************
 * arch/cosmac/src/common/cosmac_div64.c
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

/* Compact 64-bit division for the CDP1802 (ADR 0009).
 *
 * NuttX's time_t and clock_t are 64 bits, so the kernel divides 64-bit
 * values in its time conversions.  The toolchain's libgcc implements
 * __divdi3, __udivdi3 and __umoddi3 with GCC's generic C code, which
 * compiles to about 11 KB *each* on the CDP1802 (32 KB of a 90 KB image,
 * Step 04 measurement).  These replacements share one bit-at-a-time loop
 * and take about 3.5 KB together.  Because the architecture library is
 * linked before libgcc, these definitions are the ones used.
 *
 * When both operands fit in 32 bits (the common case: tick and nanosecond
 * conversions), the 32-bit division from libgcc's hand-written assembly is
 * used instead of the 64-iteration loop.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stddef.h>
#include <stdint.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

uint64_t __udivdi3(uint64_t n, uint64_t d);
uint64_t __umoddi3(uint64_t n, uint64_t d);
int64_t __divdi3(int64_t a, int64_t b);
int64_t __moddi3(int64_t a, int64_t b);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: udivmod64
 *
 * Description:
 *   Unsigned 64-bit division; the remainder is returned in *rem if rem is
 *   not NULL.  Division by zero returns an all-ones quotient, like the
 *   restoring algorithm naturally does (the C standard leaves it undefined).
 *
 ****************************************************************************/

static uint64_t noinline_function udivmod64(uint64_t n, uint64_t d,
                                            FAR uint64_t *rem)
{
  uint64_t q = 0;
  uint64_t r = 0;
  int i;

  if ((uint32_t)(n >> 32) == 0 && (uint32_t)(d >> 32) == 0 && d != 0)
    {
      /* Both operands fit in 32 bits */

      uint32_t n32 = (uint32_t)n;
      uint32_t d32 = (uint32_t)d;

      if (rem != NULL)
        {
          *rem = n32 % d32;
        }

      return n32 / d32;
    }

  for (i = 63; i >= 0; i--)
    {
      r = (r << 1) | ((n >> i) & 1);
      if (r >= d)
        {
          r -= d;
          q |= (uint64_t)1 << i;
        }
    }

  if (rem != NULL)
    {
      *rem = r;
    }

  return q;
}

/****************************************************************************
 * Name: neg64_if
 *
 * Description:
 *   Return -v if neg is non-zero, v otherwise (one shared copy of the
 *   64-bit negation, which is expensive on an 8-bit CPU).
 *
 ****************************************************************************/

static uint64_t noinline_function neg64_if(uint64_t v, int neg)
{
  return neg ? (uint64_t)0 - v : v;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

uint64_t __udivdi3(uint64_t n, uint64_t d)
{
  return udivmod64(n, d, NULL);
}

uint64_t __umoddi3(uint64_t n, uint64_t d)
{
  uint64_t r;

  udivmod64(n, d, &r);
  return r;
}

int64_t __divdi3(int64_t a, int64_t b)
{
  int na = a < 0;
  int nb = b < 0;

  return (int64_t)neg64_if(udivmod64(neg64_if(a, na), neg64_if(b, nb),
                                     NULL), na ^ nb);
}

int64_t __moddi3(int64_t a, int64_t b)
{
  uint64_t r;
  int na = a < 0;

  udivmod64(neg64_if(a, na), neg64_if(b, b < 0), &r);
  return (int64_t)neg64_if(r, na);
}
