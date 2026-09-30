/****************************************************************************
 * boards/cosmac/cdp1802/sim1802/src/sim1802_serial.c
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

/* The sim1802 console as a serial device (lower half of NuttX's serial
 * driver): /dev/ttyS0, and /dev/console with CONFIG_UART0_SERIAL_CONSOLE.
 *
 * Output: the simulator's PUTCHAR command writes a byte at once, so the
 * transmitter is always ready and there is no transmit interrupt; the
 * upper half's data is sent as soon as it asks for a transmit interrupt.
 *
 * Input: IRQ 6 is pending while input is available (level-triggered;
 * STATUS bit 0), GETCHAR reads a byte.  When the receive buffer fills up,
 * the IRQ is masked (input flow control, CONFIG_SERIAL_IFLOWCONTROL): the
 * simulator keeps the rest of the input, like a sender stopped by RTS, and
 * nothing is lost.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/serial/serial.h>

#include "cosmac_internal.h"
#include "sim1802_io.h"

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  sim1802_setup(FAR struct uart_dev_s *dev);
static void sim1802_shutdown(FAR struct uart_dev_s *dev);
static int  sim1802_attach(FAR struct uart_dev_s *dev);
static void sim1802_detach(FAR struct uart_dev_s *dev);
static int  sim1802_ioctl(FAR struct file *filep, int cmd,
                          unsigned long arg);
static int  sim1802_receive(FAR struct uart_dev_s *dev,
                            FAR unsigned int *status);
static void sim1802_rxint(FAR struct uart_dev_s *dev, bool enable);
static bool sim1802_rxavailable(FAR struct uart_dev_s *dev);
#ifdef CONFIG_SERIAL_IFLOWCONTROL
static bool sim1802_rxflowcontrol(FAR struct uart_dev_s *dev,
                                  unsigned int nbuffered, bool upper);
#endif
static void sim1802_send(FAR struct uart_dev_s *dev, int ch);
static void sim1802_txint(FAR struct uart_dev_s *dev, bool enable);
static bool sim1802_txready(FAR struct uart_dev_s *dev);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct uart_ops_s g_sim1802_ops =
{
  .setup          = sim1802_setup,
  .shutdown       = sim1802_shutdown,
  .attach         = sim1802_attach,
  .detach         = sim1802_detach,
  .ioctl          = sim1802_ioctl,
  .receive        = sim1802_receive,
  .rxint          = sim1802_rxint,
  .rxavailable    = sim1802_rxavailable,
#ifdef CONFIG_SERIAL_IFLOWCONTROL
  .rxflowcontrol  = sim1802_rxflowcontrol,
#endif
  .send           = sim1802_send,
  .txint          = sim1802_txint,
  .txready        = sim1802_txready,
  .txempty        = sim1802_txready,
};

static char g_sim1802_rxbuffer[CONFIG_UART0_RXBUFSIZE];
static char g_sim1802_txbuffer[CONFIG_UART0_TXBUFSIZE];

static uart_dev_t g_sim1802_uart =
{
#ifdef CONFIG_UART0_SERIAL_CONSOLE
  .isconsole = true,
#endif
  .recv      =
  {
    .size    = CONFIG_UART0_RXBUFSIZE,
    .buffer  = g_sim1802_rxbuffer,
  },
  .xmit      =
  {
    .size    = CONFIG_UART0_TXBUFSIZE,
    .buffer  = g_sim1802_txbuffer,
  },
  .ops       = &g_sim1802_ops,
};

/* The upper half wants receive interrupts / flow control stopped them */

static bool g_sim1802_rxint;
static bool g_sim1802_rxstopped;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void sim1802_rxirq_update(void)
{
  if (g_sim1802_rxint && !g_sim1802_rxstopped)
    {
      up_enable_irq(COSMAC_IRQ_CONSOLE);
    }
  else
    {
      up_disable_irq(COSMAC_IRQ_CONSOLE);
    }
}

static int sim1802_interrupt(int irq, FAR void *context, FAR void *arg)
{
  uart_recvchars((FAR uart_dev_t *)arg);
  return OK;
}

static int sim1802_setup(FAR struct uart_dev_s *dev)
{
  return OK;
}

static void sim1802_shutdown(FAR struct uart_dev_s *dev)
{
  g_sim1802_rxint = false;
  sim1802_rxirq_update();
}

static int sim1802_attach(FAR struct uart_dev_s *dev)
{
  return irq_attach(COSMAC_IRQ_CONSOLE, sim1802_interrupt, dev);
}

static void sim1802_detach(FAR struct uart_dev_s *dev)
{
  up_disable_irq(COSMAC_IRQ_CONSOLE);
  irq_detach(COSMAC_IRQ_CONSOLE);
}

static int sim1802_ioctl(FAR struct file *filep, int cmd,
                         unsigned long arg)
{
  return -ENOTTY;
}

static int sim1802_receive(FAR struct uart_dev_s *dev,
                           FAR unsigned int *status)
{
  irqstate_t flags = up_irq_save();
  uint8_t ch = sim1802_query(SIM1802_CMD_CONSOLE_GETCHAR);

  up_irq_restore(flags);
  *status = 0;
  return ch;
}

static void sim1802_rxint(FAR struct uart_dev_s *dev, bool enable)
{
  irqstate_t flags = up_irq_save();

  g_sim1802_rxint = enable;
  sim1802_rxirq_update();
  up_irq_restore(flags);
}

static bool sim1802_rxavailable(FAR struct uart_dev_s *dev)
{
  irqstate_t flags = up_irq_save();
  uint8_t status = sim1802_query(SIM1802_CMD_CONSOLE_STATUS);

  up_irq_restore(flags);
  return (status & SIM1802_CONSOLE_AVAILABLE) != 0;
}

#ifdef CONFIG_SERIAL_IFLOWCONTROL
static bool sim1802_rxflowcontrol(FAR struct uart_dev_s *dev,
                                  unsigned int nbuffered, bool upper)
{
  irqstate_t flags = up_irq_save();

  /* upper: the buffer is full, stop reading (the input waits in the
   * simulator); otherwise it has been drained, take input again.
   */

  g_sim1802_rxstopped = upper;
  sim1802_rxirq_update();
  up_irq_restore(flags);
  return upper;
}
#endif

static void sim1802_send(FAR struct uart_dev_s *dev, int ch)
{
  irqstate_t flags = up_irq_save();

  sim1802_command(SIM1802_CMD_CONSOLE_PUTCHAR, (unsigned int)ch);
  up_irq_restore(flags);
}

static void sim1802_txint(FAR struct uart_dev_s *dev, bool enable)
{
  irqstate_t flags;

  /* There is no transmit interrupt: the transmitter is always ready, so
   * send everything that is buffered now.
   */

  if (enable)
    {
      flags = enter_critical_section();
      uart_xmitchars(dev);
      leave_critical_section(flags);
    }
}

static bool sim1802_txready(FAR struct uart_dev_s *dev)
{
  return true;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: cosmac_serialinit
 *
 * Description:
 *   Register the serial device(s); called by up_initialize().
 *
 ****************************************************************************/

void cosmac_serialinit(void)
{
#ifdef CONFIG_UART0_SERIAL_CONSOLE
  uart_register("/dev/console", &g_sim1802_uart);
#endif
  uart_register("/dev/ttyS0", &g_sim1802_uart);
}
