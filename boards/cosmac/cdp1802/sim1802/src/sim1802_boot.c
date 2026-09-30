/****************************************************************************
 * boards/cosmac/cdp1802/sim1802/src/sim1802_boot.c
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

#include <syslog.h>

#include <nuttx/board.h>
#include <nuttx/fs/fs.h>
#include <nuttx/sched.h>

#include <arch/board/board.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: cdp1802_boardinitialize
 *
 * Description:
 *   Board-specific initialization, called by the reset code before
 *   nx_start().  The sim1802 board needs none yet.
 *
 ****************************************************************************/

void cdp1802_boardinitialize(void)
{
}

/****************************************************************************
 * Name: board_late_initialize
 *
 * Description:
 *   Called by nx_bringup() once the OS is running
 *   (CONFIG_BOARD_LATE_INITIALIZE): mounts procfs, which NSH's ps and free
 *   read, and with CONFIG_SIM1802_START_NSH starts NSH itself, instead of
 *   an init task started through task_spawn() (CONFIG_INIT_NONE).
 *
 ****************************************************************************/

#ifdef CONFIG_SIM1802_START_NSH
int nsh_main(int argc, FAR char *argv[]);
#endif

void board_late_initialize(void)
{
#ifdef CONFIG_FS_PROCFS
  int ret = nx_mount(NULL, "/proc", "procfs", 0, NULL);

  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: Failed to mount procfs: %d\n", ret);
    }
#endif

#ifdef CONFIG_SIM1802_START_NSH
  task_create("nsh", CONFIG_SIM1802_NSH_PRIORITY,
              CONFIG_SIM1802_NSH_STACKSIZE, nsh_main, NULL);
#endif
}
