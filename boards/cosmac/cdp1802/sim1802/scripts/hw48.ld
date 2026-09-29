/****************************************************************************
 * boards/cosmac/cdp1802/sim1802/scripts/hw48.ld
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

/* HW48/16 profile (ADR 0012): 48 KiB ROM at the reset vector 0x0000, 16 KiB
 * RAM at 0xC000.  nuttx.bin (objcopy -O binary) is the ROM image; .data is
 * copied from ROM to RAM by the reset code.
 */

OUTPUT_ARCH(cdp1802)
ENTRY(__start)

MEMORY
{
  rom (rx)  : ORIGIN = 0x0000, LENGTH = 48K
  ram (rwx) : ORIGIN = 0xC000, LENGTH = 16K
}

SECTIONS
{
  .text :
  {
    _stext = ABSOLUTE(.);
    KEEP(*(.reset))             /* reset code at 0x0000 */
    *(.text .text.*)
    *(.rodata .rodata.*)
    _etext = ABSOLUTE(.);
  } > rom

  .init_section :
  {
    _sinit = ABSOLUTE(.);
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
    KEEP(*(.init_array .ctors))
    _einit = ABSOLUTE(.);
  } > rom

  _eronly = LOADADDR(.data);    /* ROM copy of .data */

  .data :
  {
    _sdata = ABSOLUTE(.);
    *(.data .data.*)
    _edata = ABSOLUTE(.);
  } > ram AT > rom

  .bss (NOLOAD) :
  {
    _sbss = ABSOLUTE(.);
    *(.bss .bss.*)
    *(COMMON)
    _ebss = ABSOLUTE(.);
  } > ram

  /* sim1802 --rom-size (scripts/run.sh): the whole ROM window */

  __sim1802_rom_end = ORIGIN(ram);

  ASSERT(_eronly + SIZEOF(.data) <= ORIGIN(ram), "HW48/16: ROM overflow")

  /* Stabs debugging sections */

  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .comment 0 : { *(.comment) }
  .debug_abbrev 0 : { *(.debug_abbrev) }
  .debug_info 0 : { *(.debug_info) }
  .debug_line 0 : { *(.debug_line) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  .debug_aranges 0 : { *(.debug_aranges) }
}
