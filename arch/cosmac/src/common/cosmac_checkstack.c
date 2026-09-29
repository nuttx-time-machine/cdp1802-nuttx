/****************************************************************************
 * arch/cosmac/src/common/cosmac_checkstack.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

#include <nuttx/arch.h>
#include <nuttx/sched.h>

#include "cosmac_internal.h"

#ifdef CONFIG_STACK_COLORATION

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_check_tcbstack
 *
 * Description:
 *   The high-water mark of a thread's stack: the stack grows downwards
 *   from stack_base_ptr + adj_stack_size, so the used part starts at the
 *   first byte above the base that no longer holds STACK_COLOR.  The idle
 *   thread's stack is not colored (it is in use from reset).
 *
 ****************************************************************************/

size_t up_check_tcbstack(FAR struct tcb_s *tcb, size_t check_size)
{
  FAR const uint8_t *end;
  FAR const uint8_t *p;

  if (tcb->stack_base_ptr == NULL)
    {
      return 0;
    }

  if (check_size == 0 || check_size > tcb->adj_stack_size)
    {
      check_size = tcb->adj_stack_size;
    }

  /* Only the check_size bytes nearest the top are examined */

  end = (FAR const uint8_t *)tcb->stack_base_ptr + tcb->adj_stack_size;
  p   = end - check_size;
  while (p < end && *p == STACK_COLOR)
    {
      p++;
    }

  return (size_t)(end - p);
}

#endif /* CONFIG_STACK_COLORATION */
