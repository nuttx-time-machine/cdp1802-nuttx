/****************************************************************************
 * boards/cosmac/cdp1802/sim1802/src/sim1802_console.c
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

/* A minimal /dev/console for small configurations
 * (CONFIG_SIM1802_CONSOLE), instead of NuttX's serial driver (about 11 KB
 * on the CDP1802): writes go straight to the simulator's PUTCHAR command,
 * and a read waits on a semaphore that the console input interrupt
 * (IRQ 6) posts.  Input is echoed, as a serial terminal would (NSH's
 * readline leaves echo to the terminal).  No buffering, termios or flow
 * control: the simulator holds the input until it is read.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>

#include <nuttx/arch.h>
#include <nuttx/fs/fs.h>
#include <nuttx/irq.h>
#include <nuttx/semaphore.h>

#include "cosmac_internal.h"
#include "sim1802_io.h"

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static ssize_t sim1802_console_read(FAR struct file *filep,
                                    FAR char *buffer, size_t buflen);
static ssize_t sim1802_console_write(FAR struct file *filep,
                                     FAR const char *buffer, size_t buflen);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_sim1802_console_fops =
{
  NULL,                  /* open */
  NULL,                  /* close */
  sim1802_console_read,  /* read */
  sim1802_console_write, /* write */
};

/* Posted by the input interrupt, which then disables itself */

static sem_t g_sim1802_console_rxsem = SEM_INITIALIZER(0);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int sim1802_console_interrupt(int irq, FAR void *context,
                                     FAR void *arg)
{
  up_disable_irq(COSMAC_IRQ_CONSOLE);
  nxsem_post(&g_sim1802_console_rxsem);
  return OK;
}

/* Read what is available, at least one byte; 0 at the end of input */

static ssize_t sim1802_console_read(FAR struct file *filep,
                                    FAR char *buffer, size_t buflen)
{
  size_t n = 0;

  while (n < buflen)
    {
      irqstate_t flags = up_irq_save();
      uint8_t status = sim1802_query(SIM1802_CMD_CONSOLE_STATUS);

      if ((status & SIM1802_CONSOLE_AVAILABLE) != 0)
        {
          uint8_t ch = sim1802_query(SIM1802_CMD_CONSOLE_GETCHAR);

          sim1802_command(SIM1802_CMD_CONSOLE_PUTCHAR, ch);
          buffer[n++] = ch;
          up_irq_restore(flags);
          continue;
        }

      if (n > 0 || (status & SIM1802_CONSOLE_EOF) != 0)
        {
          up_irq_restore(flags);
          break;
        }

      /* Nothing yet: let the next input byte interrupt, and sleep */

      up_enable_irq(COSMAC_IRQ_CONSOLE);
      up_irq_restore(flags);
      nxsem_wait_uninterruptible(&g_sim1802_console_rxsem);
    }

  return n;
}

static ssize_t sim1802_console_write(FAR struct file *filep,
                                     FAR const char *buffer, size_t buflen)
{
  size_t i;

  for (i = 0; i < buflen; i++)
    {
      irqstate_t flags = up_irq_save();

      sim1802_command(SIM1802_CMD_CONSOLE_PUTCHAR, (uint8_t)buffer[i]);
      up_irq_restore(flags);
    }

  return buflen;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: cosmac_serialinit
 *
 * Description:
 *   Called by up_initialize(): register /dev/console.
 *
 ****************************************************************************/

void cosmac_serialinit(void)
{
  irq_attach(COSMAC_IRQ_CONSOLE, sim1802_console_interrupt, NULL);
  register_driver("/dev/console", &g_sim1802_console_fops, 0666, NULL);
}
