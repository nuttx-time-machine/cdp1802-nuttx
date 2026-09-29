##############################################################################
# arch/cosmac/src/Makefile
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

include $(TOPDIR)/tools/Make.defs
include chip/Make.defs

# The CDP1802 is the only COSMAC chip supported, so there is no family
# subdirectory: shared code lives in common/, chip code in the chip/ link
# (arch/cosmac/src/cdp1802).

ARCH_SUBDIR = common

ARCH_SRCDIR = $(TOPDIR)$(DELIM)arch$(DELIM)$(CONFIG_ARCH)$(DELIM)src

INCLUDES += ${INCDIR_PREFIX}$(ARCH_SRCDIR)$(DELIM)chip
INCLUDES += ${INCDIR_PREFIX}$(ARCH_SRCDIR)$(DELIM)common
INCLUDES += ${INCDIR_PREFIX}$(ARCH_SRCDIR)$(DELIM)$(ARCH_SUBDIR)
INCLUDES += ${INCDIR_PREFIX}$(TOPDIR)$(DELIM)sched

CPPFLAGS += $(INCLUDES)
CFLAGS += $(INCLUDES)
CXXFLAGS += $(INCLUDES)
AFLAGS += $(INCLUDES)

NUTTX = $(call CONVERT_PATH,$(TOPDIR)$(DELIM)nuttx$(EXEEXT))

HEAD_OBJ = $(HEAD_ASRC:.S=$(OBJEXT))
STARTUP_OBJS ?= $(HEAD_OBJ)

ASRCS = $(CHIP_ASRCS) $(CMN_ASRCS)
AOBJS = $(ASRCS:.S=$(OBJEXT))

CSRCS = $(CHIP_CSRCS) $(CMN_CSRCS)
COBJS = $(CSRCS:.c=$(OBJEXT))

SRCS = $(ASRCS) $(CSRCS)
OBJS = $(AOBJS) $(COBJS)

# Override in Make.defs if linker is not 'ld'

LDSTARTGROUP ?= --start-group
LDENDGROUP ?= --end-group
LDFLAGS += $(addprefix -T,$(call CONVERT_PATH,$(ARCHSCRIPT)))

BOARDMAKE = $(if $(wildcard board$(DELIM)Makefile),y,)

LIBPATHS += -L $(call CONVERT_PATH,$(TOPDIR)$(DELIM)staging)
ifeq ($(BOARDMAKE),y)
  LIBPATHS += -L $(call CONVERT_PATH,$(TOPDIR)$(DELIM)arch$(DELIM)$(CONFIG_ARCH)$(DELIM)src$(DELIM)board)
endif

LDLIBS = $(patsubst %.a,%,$(patsubst lib%,-l%,$(LINKLIBS)))
ifeq ($(BOARDMAKE),y)
  LDLIBS += -lboard
endif

VPATH = chip:common:$(ARCH_SUBDIR)

all: $(HEAD_OBJ) libarch$(LIBEXT)

.PHONY: board/libboard$(LIBEXT)

$(AOBJS) $(HEAD_OBJ): %$(OBJEXT): %.S
	$(call ASSEMBLE, $<, $@)

$(COBJS): %$(OBJEXT): %.c
	$(call COMPILE, $<, $@)

libarch$(LIBEXT): $(OBJS)
	$(call ARCHIVE_REBUILD, $@, $(OBJS))

board/libboard$(LIBEXT):
	$(Q) $(MAKE) -C board libboard$(LIBEXT) EXTRAFLAGS="$(EXTRAFLAGS)"

ifeq ($(CONFIG_COSMAC_BANKING),y)

# Code banking (ADR 0013 of the port).  A measuring link records the code
# each object keeps after --gc-sections; tools/cosmac/cosmac_bank.py places
# objects in banks by the board's rules ($(COSMAC_BANK_RULES)) and writes
# renamed copies of the libraries, the far-call stubs and banks.ld, which
# the board's linker script INCLUDEs; then the real link uses those
# libraries first.  The placement report is nuttx.banks.

BANKDIR = $(ARCH_SRCDIR)$(DELIM)bank
BANKLIBS = $(addprefix $(TOPDIR)$(DELIM)staging$(DELIM),$(LINKLIBS))
MEASURE_LDFLAGS = $(filter-out -T% -Map=%,$(LDFLAGS))

nuttx$(EXEEXT): $(HEAD_OBJ) board/libboard$(LIBEXT) $(ARCHSCRIPT) $(COSMAC_BANK_RULES)
	@echo "LD: nuttx (measuring link for bank placement)"
	$(Q) mkdir -p $(BANKDIR)
	$(Q) $(LD) --entry=__start $(MEASURE_LDFLAGS) --noinhibit-exec \
		-T $(ARCH_SRCDIR)$(DELIM)common$(DELIM)cosmac_measure.ld \
		-Map=$(BANKDIR)$(DELIM)measure.map $(LIBPATHS) $(EXTRA_LIBPATHS) \
		-o $(BANKDIR)$(DELIM)measure.elf $(HEAD_OBJ) $(EXTRA_OBJS) \
		$(LDSTARTGROUP) $(LDLIBS) $(EXTRA_LIBS) $(LDENDGROUP) \
		> $(BANKDIR)$(DELIM)measure.log 2>&1 || true
	$(Q) test -s $(BANKDIR)$(DELIM)measure.map
	@echo "BANK: $(notdir $(COSMAC_BANK_RULES))"
	$(Q) python3 $(TOPDIR)$(DELIM)tools$(DELIM)cosmac$(DELIM)cosmac_bank.py \
		--rules $(COSMAC_BANK_RULES) --banks $(CONFIG_COSMAC_BANK_COUNT) \
		--map $(BANKDIR)$(DELIM)measure.map \
		--outdir $(BANKDIR)$(DELIM)libs --stubs $(BANKDIR)$(DELIM)stubs.S \
		--ldscript $(BANKDIR)$(DELIM)libs$(DELIM)banks.ld \
		--report $(TOPDIR)$(DELIM)nuttx.banks \
		--objcopy $(OBJCOPY) --ar $(CROSSDEV)ar $(BANKLIBS)
	$(Q) $(CC) -c -o $(BANKDIR)$(DELIM)stubs.o $(BANKDIR)$(DELIM)stubs.S
	@echo "LD: nuttx"
	$(Q) $(LD) --entry=__start -L $(BANKDIR)$(DELIM)libs $(LDFLAGS) \
		$(LIBPATHS) $(EXTRA_LIBPATHS) \
		-o $(NUTTX) $(HEAD_OBJ) $(BANKDIR)$(DELIM)stubs.o $(EXTRA_OBJS) \
		$(LDSTARTGROUP) $(LDLIBS) $(EXTRA_LIBS) $(LDENDGROUP)
else
nuttx$(EXEEXT): $(HEAD_OBJ) board/libboard$(LIBEXT) $(ARCHSCRIPT)
	@echo "LD: nuttx"
	$(Q) $(LD) --entry=__start $(LDFLAGS) $(LIBPATHS) $(EXTRA_LIBPATHS) \
		-o $(NUTTX) $(HEAD_OBJ) $(EXTRA_OBJS) \
		$(LDSTARTGROUP) $(LDLIBS) $(EXTRA_LIBS) $(LDENDGROUP)
endif
ifneq ($(CONFIG_WINDOWS_NATIVE),y)
	$(Q) $(NM) $(NUTTX) | \
	grep -v '\(compiled\)\|\(\$(OBJEXT)$$\)\|\( [aUw] \)\|\(\.\.ng$$\)\|\(LASH[RL]DI\)' | \
	sort > $(TOPDIR)/System.map
endif

# This is part of the top-level export target

export_startup: $(STARTUP_OBJS)
	$(Q) if [ -d "$(EXPORT_DIR)/startup" ]; then \
		cp -f $(STARTUP_OBJS) "$(EXPORT_DIR)/startup"; \
	 else \
		echo "$(EXPORT_DIR)/startup does not exist"; \
		exit 1; \
	 fi

# Dependencies

makedepfile: $(CSRCS:.c=.ddc) $(ASRCS:.S=.dds) $(HEAD_ASRC:.S=.dds)
	$(call CATFILE, Make.dep, $^)
	$(call DELFILE, $^)

.depend: Makefile chip/Make.defs $(SRCS) $(TOPDIR)$(DELIM).config
ifeq ($(BOARDMAKE),y)
	$(Q) $(MAKE) -C board depend
endif
	$(Q) $(MAKE) makedepfile DEPPATH="--dep-path chip --dep-path common --dep-path $(ARCH_SUBDIR)"
	$(Q) touch $@

depend: .depend

context::

clean:
ifeq ($(BOARDMAKE),y)
	$(Q) $(MAKE) -C board clean
endif
	$(call DELFILE, libarch$(LIBEXT))
	$(call DELDIR, bank)
	$(call CLEAN)

distclean:: clean
ifeq ($(BOARDMAKE),y)
	$(Q) $(MAKE) -C board distclean
endif
	$(call DELFILE, $(CSRCS:.c=.ddc) $(ASRCS:.S=.dds) $(HEAD_ASRC:.S=.dds))
	$(call DELFILE, Make.dep)
	$(call DELFILE, .depend)

-include Make.dep
