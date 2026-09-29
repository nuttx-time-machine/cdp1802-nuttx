/****************************************************************************
 * arch/cosmac/src/common/cosmac_createstack.c
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

#include <string.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/kmalloc.h>
#include <nuttx/sched.h>

#include "cosmac_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_create_stack
 *
 * Description:
 *   Allocate a stack for a new thread and setup up stack-related
 *   information in the TCB.  Stacks are byte aligned on the CDP1802 and
 *   grow downwards.
 *
 ****************************************************************************/

int up_create_stack(FAR struct tcb_s *tcb, size_t stack_size, uint8_t ttype)
{
  /* A stack of a different size is released first */

  if (tcb->stack_alloc_ptr && tcb->adj_stack_size != stack_size)
    {
      up_release_stack(tcb, ttype);
    }

  if (tcb->stack_alloc_ptr == NULL)
    {
#ifdef CONFIG_MM_KERNEL_HEAP
      if (ttype == TCB_FLAG_TTYPE_KERNEL)
        {
          tcb->stack_alloc_ptr = kmm_malloc(stack_size);
        }
      else
#endif
        {
          tcb->stack_alloc_ptr = kumm_malloc(stack_size);
        }

      if (tcb->stack_alloc_ptr == NULL)
        {
          serr("ERROR: Failed to allocate stack, size %u\n",
               (unsigned int)stack_size);
          return ERROR;
        }
    }

#ifdef CONFIG_STACK_COLORATION
  memset(tcb->stack_alloc_ptr, STACK_COLOR, stack_size);
#endif

  tcb->stack_base_ptr = tcb->stack_alloc_ptr;
  tcb->adj_stack_size = stack_size;
  tcb->flags         |= TCB_FLAG_FREE_STACK;
  return OK;
}

/****************************************************************************
 * Name: up_use_stack
 *
 * Description:
 *   Setup stack-related information in the TCB using pre-allocated stack
 *   memory.
 *
 ****************************************************************************/

int up_use_stack(FAR struct tcb_s *tcb, FAR void *stack, size_t stack_size)
{
  if (tcb->stack_alloc_ptr)
    {
      up_release_stack(tcb, tcb->flags & TCB_FLAG_TTYPE_MASK);
    }

  tcb->stack_alloc_ptr = stack;

#ifdef CONFIG_STACK_COLORATION
  memset(tcb->stack_alloc_ptr, STACK_COLOR, stack_size);
#endif

  tcb->stack_base_ptr = tcb->stack_alloc_ptr;
  tcb->adj_stack_size = stack_size;
  return OK;
}

/****************************************************************************
 * Name: up_release_stack
 *
 * Description:
 *   A task has been stopped.  Free all stack related resources retained in
 *   the defunct TCB.
 *
 ****************************************************************************/

void up_release_stack(FAR struct tcb_s *dtcb, uint8_t ttype)
{
  if (dtcb->stack_alloc_ptr && (dtcb->flags & TCB_FLAG_FREE_STACK))
    {
#ifdef CONFIG_MM_KERNEL_HEAP
      if (ttype == TCB_FLAG_TTYPE_KERNEL)
        {
          kmm_free(dtcb->stack_alloc_ptr);
        }
      else
#endif
        {
          kumm_free(dtcb->stack_alloc_ptr);
        }
    }

  dtcb->flags          &= ~TCB_FLAG_FREE_STACK;
  dtcb->stack_alloc_ptr = NULL;
  dtcb->stack_base_ptr  = NULL;
  dtcb->adj_stack_size  = 0;
}
