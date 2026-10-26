# ============================================================
# San Francisco Rush 2049 (N64, US) decompilation
# ============================================================
#
#   make setup     download and sha256-check IDO 5.3 / 7.1 into tools/ido/
#   make extract   cut/inflate the ROM-derived inputs from baserom.us.z64
#   make           build build/us/rush2049.us.z64 and check its SHA-1
#
# The cartridge has two kinds of code:
#   * static boot/library code, linked in place: src/rom/*.c (C, with the few
#     original hand-written assembly routines as GLOBAL_ASM) and asm/us/*.s;
#   * three compressed code images inside the data segment: the game code
#     (0x80086A50) and runtime images A/B (0x8038A400). Each is linked from
#     src/blob, src/ovl_a, src/ovl_b (C) plus asm/us/{blob,ovl_a,ovl_b}
#     (functions not yet in C), byte-checked, and recompressed with zlib 1.0.4.
#     Only the DATA of the three retail images is read from the ROM; every code
#     word comes from src/ or asm/us/, and each compiled function's own
#     literals/tables go to the addresses frozen in manifest/<image>.json.
#
# Toolchain: IDO 5.3 and IDO 7.1 (ido-static-recomp), mips-linux-gnu binutils,
# a host C compiler, Python 3.9+. See README.md.

VERSION      := us
BUILD_DIR    := build/$(VERSION)
SRC_DIR      := src
ASM_DIR      := asm/$(VERSION)
ASSETS_DIR   := assets/$(VERSION)
BASEROM      := baserom.$(VERSION).z64
TARGET       := $(BUILD_DIR)/rush2049.$(VERSION).z64
ELF          := $(BUILD_DIR)/rush2049.$(VERSION).elf
LD_SCRIPT    := rush2049.$(VERSION).ld
COMPARE      ?= 1
VERBOSE      ?= 0
COMPILER     := ido

# --- tools ---------------------------------------------------------------
CROSS        ?= mips-linux-gnu-
AS           := $(CROSS)as
LD           := $(CROSS)ld
OBJCOPY      := $(CROSS)objcopy
PYTHON       ?= python3
HOSTCC       ?= cc

# IDO 5.3 and 7.1 static recompilations (decompals/ido-static-recomp release
# archives unpacked here, or point the variables elsewhere).
IDO53_DIR    ?= tools/ido/5.3
IDO71_DIR    ?= tools/ido/7.1
IDO53_CC     := $(IDO53_DIR)/cc
CC           := $(IDO53_CC)
IDO71_CC     := $(IDO71_DIR)/cc
# IDO 5.3 through `cc -S`, strip `.loc`, as1 -O1 (the inflate TU, lib_5610)
IDO53S_CC    := tools/ido_sas/cc
export IDO_SAS_REAL_CC := $(abspath $(IDO53_CC))
MIPSISET     := -mips2
OPT_FLAGS    := -O2

ASFLAGS      := -march=vr4300 -mabi=32 -Iinclude
INCLUDE_CFLAGS := -Iinclude -Iinclude/PR -D_LANGUAGE_C
CFLAGS       := -G 0 $(MIPSISET) $(OPT_FLAGS) -non_shared $(INCLUDE_CFLAGS) \
                -Wab,-r4300_mul -Xcpluscomm

SYMS_LD      := $(BUILD_DIR)/syms.ld
LDFLAGS      := -T $(LD_SCRIPT) -T $(SYMS_LD) -T hardware_regs.ld -T rom_symbol_aliases.ld \
                -Map $(BUILD_DIR)/rush2049.$(VERSION).map --no-check-sections \
                --accept-unknown-input-arch

ifeq ($(VERBOSE),0)
  V := @
endif

# --- inputs ----------------------------------------------------------------
ASM_FILES    := $(wildcard $(ASM_DIR)/*.s)
ASM_O_FILES  := $(patsubst $(ASM_DIR)/%.s,$(BUILD_DIR)/$(ASM_DIR)/%.o,$(ASM_FILES))
BIN_O_FILES  := $(BUILD_DIR)/$(ASSETS_DIR)/ipl3.o $(BUILD_DIR)/$(ASSETS_DIR)/data.o
SRC_ROM_C    := $(wildcard $(SRC_DIR)/rom/*.c)
SRC_ROM_O    := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/$(SRC_DIR)/%.o,$(SRC_ROM_C))
O_FILES      := $(ASM_O_FILES) $(BIN_O_FILES) $(SRC_ROM_O)

# ROM-derived inputs (tools/extract_rom.py); never shipped.
EXTRACTED    := build/.extracted
GAME_IMAGE   := build/game_code.bin
OVL_A_IMAGE  := build/ovl/a/image.bin
OVL_B_IMAGE  := build/ovl/b/image.bin

.PHONY: all setup extract verify clean distclean
.DEFAULT_GOAL := all

all: $(TARGET)
ifeq ($(COMPARE),1)
	@$(MAKE) --no-print-directory verify
endif

setup:
	$(V)$(PYTHON) tools/setup_ido.py

extract: $(EXTRACTED)

$(EXTRACTED): $(BASEROM) tools/extract_rom.py
	$(V)$(PYTHON) tools/extract_rom.py $(BASEROM)
	@mkdir -p $(@D) && touch $@

$(ASSETS_DIR)/ipl3.bin $(ASSETS_DIR)/data.bin $(GAME_IMAGE) $(OVL_A_IMAGE) $(OVL_B_IMAGE): $(EXTRACTED)

# --- static code -------------------------------------------------------------
$(SYMS_LD): undefined_syms_auto.$(VERSION).txt undefined_funcs_auto.$(VERSION).txt
	@mkdir -p $(@D)
	$(V)cat $^ > $@

$(BUILD_DIR)/$(ASM_DIR)/%.o: $(ASM_DIR)/%.s
	@mkdir -p $(@D)
	@echo "AS $<"
	$(V)$(AS) $(ASFLAGS) -o $@ $<

# ROM-aligned TUs: asm-processor injects the GLOBAL_ASM routines into the IDO
# object. A TU built at IDO `-mips3 -32` gets EF_MIPS_32BITMODE (o32abi.py) so
# GNU ld links it beside the -mips2 objects. opt_overrides.mk selects per-TU
# flags and compilers (IDO 7.1 for the boot-tail TUs); a missing compiler fails.
$(BUILD_DIR)/$(SRC_DIR)/rom/%.o: $(SRC_DIR)/rom/%.c
	@mkdir -p $(@D)
	@echo "CC $<$(if $(filter $(IDO71_CC),$(CC)), (IDO 7.1),)$(if $(filter $(IDO53S_CC),$(CC)), (IDO 5.3 -S/as1 -O1),)"
	@test -x $(CC) || { echo "error: compiler $(CC) for $< is not installed (see README.md)"; exit 1; }
	$(V)$(PYTHON) tools/asm-processor/build.py \
	    $(CC) -- $(AS) $(ASFLAGS) -- -c $(CFLAGS) -o $@ $<
	$(if $(filter -mips3,$(CFLAGS)),$(V)$(PYTHON) tools/o32abi.py $@ --cflags '$(CFLAGS)')
	$(V)$(PYTHON) tools/owned_data.py object $< $@

-include $(SRC_DIR)/rom/opt_overrides.mk
-include $(SRC_DIR)/rom/storage_overrides.mk
$(SRC_ROM_O): $(SRC_DIR)/rom/opt_overrides.mk include/PR/os_thread.h rom_owned_data.json

# The boot segment tail's hand-written routines, composed into their data.bin slots.
BOOT_DIR          := $(BUILD_DIR)/boot_data
BOOT_KERNELS      := $(BOOT_DIR)/kernels.bin
BOOT_C5E0         := $(BOOT_DIR)/func_8002C5E0.bin
BOOT_BINS         := $(BOOT_KERNELS) $(BOOT_C5E0)
BOOT_COMPOSE_ARGS := --extra $(BOOT_KERNELS):0x0:0x600 \
                     --extra $(BOOT_C5E0):0x4E10:0x1C
$(BOOT_KERNELS): BOOT_VADDR := 0x800277D0
$(BOOT_C5E0): BOOT_VADDR := 0x8002C5E0

$(BOOT_DIR)/%.bin: $(ASM_DIR)/boot_data/%.s tools/boot_data_bin.py
	@mkdir -p $(@D)
	@echo "AS $< (boot segment tail)"
	$(V)$(AS) $(ASFLAGS) -o $(@:.bin=.o) $<
	$(V)$(LD) -e $(BOOT_VADDR) -Ttext=$(BOOT_VADDR) -o $(@:.bin=.elf) $(@:.bin=.o)
	$(V)$(PYTHON) tools/boot_data_bin.py $(@:.bin=.elf) $@ --source $*.s --objcopy $(OBJCOPY)

# --- compressed code images ----------------------------------------------------
# Per-function compile rules from manifest/*.json.
build/rules.mk: tools/gen_rules.py manifest/game.json manifest/ovl_a.json manifest/ovl_b.json
	@mkdir -p $(@D)
	$(V)$(PYTHON) tools/gen_rules.py > $@
include build/rules.mk

DEFLATE      := build/tools/deflate104
$(DEFLATE): tools/deflate104/deflate104.c $(wildcard tools/zlib-1.0.4/*.[ch])
	@mkdir -p $(@D)
	$(V)$(HOSTCC) -O2 -w -Itools/zlib-1.0.4 -o $@ tools/deflate104/deflate104.c \
	    tools/zlib-1.0.4/deflate.c tools/zlib-1.0.4/trees.c tools/zlib-1.0.4/zutil.c \
	    tools/zlib-1.0.4/adler32.c

ROMKIT       := $(wildcard tools/romkit/*.py) tools/build_image.py
GAME_BLOB    := build/blob/game_code.deflate
OVL_A_STREAM := build/ovl/ovl_a.deflate
OVL_B_STREAM := build/ovl/ovl_b.deflate

$(GAME_BLOB): $(GAME_OBJS) $(GAME_IMAGE) layout/game.json manifest/game.json $(wildcard asm/us/blob/*) $(DEFLATE) $(ROMKIT)
	@echo "IMAGE game code (0x80086A50)"
	$(V)$(PYTHON) tools/build_image.py game --deflate $(DEFLATE) --out $@
$(OVL_A_STREAM): $(OVL_A_OBJS) $(OVL_A_IMAGE) layout/ovl_a.json manifest/ovl_a.json $(wildcard asm/us/ovl_a/*) $(DEFLATE) $(ROMKIT)
	@echo "IMAGE runtime image A (0x8038A400)"
	$(V)$(PYTHON) tools/build_image.py ovl_a --deflate $(DEFLATE) --out $@
$(OVL_B_STREAM): $(OVL_B_OBJS) $(OVL_B_IMAGE) layout/ovl_b.json manifest/ovl_b.json $(wildcard asm/us/ovl_b/*) $(DEFLATE) $(ROMKIT)
	@echo "IMAGE runtime image B (0x8038A400)"
	$(V)$(PYTHON) tools/build_image.py ovl_b --deflate $(DEFLATE) --out $@

# Maintenance: re-derive every compiled function's own-data placement from the
# retail image (this reads retail code words) and record it in manifest/*.json.
# Needed after adding or changing a function whose literals, jump tables or
# statics move; a normal build uses the recorded placements only.
.PHONY: freeze-placements
freeze-placements: $(GAME_OBJS) $(OVL_A_OBJS) $(OVL_B_OBJS) $(GAME_IMAGE) $(OVL_A_IMAGE) $(OVL_B_IMAGE) $(DEFLATE)
	$(V)$(PYTHON) tools/build_image.py game --freeze-placements --deflate $(DEFLATE) --out $(GAME_BLOB)
	$(V)$(PYTHON) tools/build_image.py ovl_a --freeze-placements --deflate $(DEFLATE) --out $(OVL_A_STREAM)
	$(V)$(PYTHON) tools/build_image.py ovl_b --freeze-placements --deflate $(DEFLATE) --out $(OVL_B_STREAM)

# data.bin offsets of the three streams (ROM offset - 0x283D0) and their lengths.
STREAM_COMPOSE_ARGS := --extra $(OVL_A_STREAM):0xB34164:80272 \
                       --extra $(OVL_B_STREAM):0xB47AF4:23639

# --- data segment, link, ROM -----------------------------------------------------
$(BUILD_DIR)/$(ASSETS_DIR)/data.o: $(ASSETS_DIR)/data.bin $(GAME_BLOB) $(OVL_A_STREAM) $(OVL_B_STREAM) $(BOOT_BINS) tools/compose_data.py tools/owned_data.py rom_owned_data.json
	@mkdir -p $(@D)
	@echo "BIN $< + game code, runtime images A/B and boot-tail routines built from source"
	$(V)$(PYTHON) tools/compose_data.py $< $(GAME_BLOB) $(BUILD_DIR)/$(ASSETS_DIR)/data.composed.bin \
	    --slot 0xAE4740 --length 326180 $(STREAM_COMPOSE_ARGS) $(BOOT_COMPOSE_ARGS)
	$(V)$(PYTHON) tools/owned_data.py split $< $(BUILD_DIR)/$(ASSETS_DIR)/data.composed.bin $@ --objcopy $(OBJCOPY)

$(BUILD_DIR)/$(ASSETS_DIR)/ipl3.o: $(ASSETS_DIR)/ipl3.bin tools/owned_data.py rom_owned_data.json
	@mkdir -p $(@D)
	@echo "BIN $<"
	$(V)$(PYTHON) tools/owned_data.py split $< $< $@ --objcopy $(OBJCOPY)

$(ELF): $(O_FILES) $(LD_SCRIPT) $(SYMS_LD) rom_symbol_aliases.ld hardware_regs.ld
	@echo "LD $@"
	$(V)$(LD) $(LDFLAGS) -o $@ $(O_FILES)

$(TARGET): $(ELF)
	@echo "OBJCOPY $@"
	$(V)$(OBJCOPY) -O binary $< $@

verify: $(TARGET)
	@echo "$$(cut -d' ' -f1 $(VERSION).sha1)  $(TARGET)" | sha1sum -c - \
	    && echo "ROM matches!" || { echo "ROM does NOT match"; exit 1; }

clean:
	$(V)rm -rf build/us build/game build/ovl_a build/ovl_b build/blob build/ovl/ovl_*.deflate build/rules.mk build/tools

distclean:
	$(V)rm -rf build assets
