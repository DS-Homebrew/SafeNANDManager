# SPDX-License-Identifier: CC0-1.0
#
# SPDX-FileContributor: Antonio Niño Díaz, 2023

BLOCKSDS	?= /opt/blocksds/core
BLOCKSDSEXT	?= /opt/blocksds/external

# User config
# ===========

NAME		:= SafeNANDManager

GAME_TITLE	:= Safe NAND Manager
GAME_SUBTITLE1 := Dump or Restore DSi NAND
GAME_SUBTITLE2 := zoogie & Rocket Robz
GAME_ICON	= $(BLOCKSDS)/sys/icon.bmp

# Tools
# -----

MAKE		:= make
RM		:= rm -rf

# Verbose flag
# ------------

ifeq ($(VERBOSE),1)
V		:=
else
V		:= @
endif

# Directories
# -----------

ARM9DIR		:= arm9
ARM7DIR		:= arm7

# Build artfacts
# --------------

ROM		:= $(NAME).dsi

# Targets
# -------

.PHONY: all clean arm9 arm7

all: $(ROM)

clean:
	@echo "  CLEAN"
	$(V)$(MAKE) -f Makefile.arm9 clean --no-print-directory
	$(V)$(MAKE) -f Makefile.arm7 clean --no-print-directory
	$(V)$(RM) $(ROM) build ntrboot.nds boot.nds

arm9:
	$(V)+$(MAKE) -f Makefile.arm9 --no-print-directory

arm7:
	$(V)+$(MAKE) -f Makefile.arm7 --no-print-directory

dump:
	$(V)+$(MAKE) -f Makefile.arm7 --no-print-directory dump
	$(V)+$(MAKE) -f Makefile.arm9 --no-print-directory dump

# Combine the title strings
GAME_FULL_TITLE := $(GAME_TITLE);$(GAME_SUBTITLE1);$(GAME_SUBTITLE2)

$(ROM): arm9 arm7
	@echo "  NDSTOOL $@"
	$(V)$(BLOCKSDS)/tools/ndstool/ndstool -c $@ \
		-7 build/arm7.elf -9 build/arm9.elf \
		-b $(GAME_ICON) "$(GAME_FULL_TITLE)" \
		$(NDSTOOL_ARGS)
	$(V)cp $(ROM) ntrboot.nds
	$(V)cp $(ROM) boot.nds
