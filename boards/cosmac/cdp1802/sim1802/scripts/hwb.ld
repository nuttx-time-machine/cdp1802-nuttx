/****************************************************************************
 * boards/cosmac/cdp1802/sim1802/scripts/hwb.ld
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

/* HWB profile (ADR 0013): code banking.
 *   0x0000-0x7FFF  fixed ROM: reset code, every object not placed in a bank,
 *                  the far-call stubs, all read-only data and the ROM copy of
 *                  .data
 *   0x8000-0xBFFF  bank window: one 16 KiB bank of the ROM, selected by the
 *                  latch on OUT CONFIG_COSMAC_BANK_PORT
 *   0xC000-0xFFFF  RAM
 * banks.ld (the OVERLAY statement for the banks) is generated at link time
 * by tools/cosmac/cosmac_bank.py.  nuttx.bin (objcopy -O binary
 * --gap-fill 0xff) is the ROM image: the fixed part padded to 32 KiB, then
 * bank 0, bank 1, ...
 */

OUTPUT_ARCH(cdp1802)
ENTRY(__start)

MEMORY
{
  rom (rx)  : ORIGIN = 0x0000, LENGTH = 32K
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

  /* Fill the rest of the fixed ROM, so that bank 0 starts at image offset
   * 0x8000.
   */

  .rompad _eronly + SIZEOF(.data) : AT(_eronly + SIZEOF(.data))
  {
    BYTE(0xff)
    FILL(0xff)
    . = 0x8000 - (_eronly + SIZEOF(.data));
  }

  INCLUDE banks.ld

  /* sim1802 --rom-size and --banks (scripts/run.sh) */

  __sim1802_rom_end = 0x8000;
  __sim1802_banks = __cosmac_banks;

  ASSERT(_eronly + SIZEOF(.data) <= 0x8000, "HWB: fixed ROM overflow")

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
