##############################################################################
# boards/cosmac/cdp1802/sim1802/src/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations
# under the License.
#
##############################################################################

include $(TOPDIR)/Make.defs

CSRCS = sim1802_boot.c sim1802_irq.c sim1802_lowputc.c sim1802_timer.c

ifeq ($(CONFIG_BOARDCTL),y)
CSRCS += sim1802_boardctl.c
endif

ifeq ($(CONFIG_SIM1802_UART0),y)
CSRCS += sim1802_serial.c
endif

ifeq ($(CONFIG_SIM1802_CONSOLE),y)
CSRCS += sim1802_console.c
endif

ifeq ($(CONFIG_BOARD_SIM1802_TICKTEST),y)
CSRCS += sim1802_ticktest.c
endif

ifeq ($(CONFIG_BOARD_SIM1802_THREADTEST),y)
CSRCS += sim1802_threadtest.c
endif

include $(TOPDIR)/boards/Board.mk
