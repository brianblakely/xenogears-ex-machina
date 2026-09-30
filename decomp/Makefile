# Explicit per-target recipes; no implicit host compiler or invented game image.
# Run inside: nix develop path:./nix/ghidra#matching
ROOT := $(abspath ..)
PYTHON ?= python3
AS := psx-as
LD := psx-ld
OBJCOPY := psx-objcopy
CPP := psx-cpp-2.7.2
CC1 := psx-cc1-2.7.2
MASPSX := maspsx
.DEFAULT_GOAL := verify
.SUFFIXES:
.SECONDARY:
MAKEFLAGS += --no-builtin-rules
.PHONY: verify smoke split coverage clean

smoke:
	@command -v psx-as >/dev/null && command -v psx-ld >/dev/null && command -v psx-objcopy >/dev/null || { echo "Enter the matching Nix shell first." >&2; exit 2; }
	cd $(ROOT) && $(PYTHON) -m unittest tests.test_matching

ifeq ($(strip $(CONFIG)),)
verify split coverage clean:
	@echo 'Set CONFIG=targets/<target>/<target>.mk (see docs/matching.md).' >&2
	@exit 2
else
include $(CONFIG)
$(foreach var,ORIGINAL ORIGINAL_SHA256 IMAGE LINKER_SCRIPT SPLAT_CONFIG BUILD,$(if $($(var)),,$(error $(var) must be set by $(CONFIG))))

# Qualified original-compatible compilation (docs/matching.md): GCC 2.7.2 cc1
# for the R3000, ASPSX 2.79 behaviour through maspsx, GNU as for the object.
CPPFLAGS := -undef -D__GNUC__=2 -Dmips -D__mips__ -D__mips -Dpsx -D__psx__ -D__psx \
	-D_PSYQ -D__EXTENSIONS__ -D_MIPSEL -D__CHAR_UNSIGNED__ -D_LANGUAGE_C -DLANGUAGE_C \
	-lang-c -nostdinc -I$(ROOT)/decomp/include $(TARGET_CPPFLAGS)
CC1FLAGS := -quiet -mcpu=3000 -fgnu-linker -mgas -msoft-float -O2
ASFLAGS := -EL -march=r3000 -mtune=r3000 -msoft-float -no-pad-sections -G0 \
	-I$(ROOT)/decomp/include -I$(ROOT)
MASPSXFLAGS := --aspsx-version=2.79

# The small-data threshold is a property of each original translation unit.
gp_of = $(or $(GP_$(basename $(notdir $(1)))),0)

# Objects come from the splat-generated linker script (regenerate with `split`).
LP := (
OBJECTS = $(addprefix $(ROOT)/,$(shell sed -n 's/^ *\([^ ]*\.o\)[$(LP)].*/\1/p' $(ROOT)/$(LINKER_SCRIPT) 2>/dev/null | sort -u))

split:
	cd $(ROOT) && splat split $(SPLAT_CONFIG)

$(ROOT)/$(BUILD)/%.o: $(ROOT)/%.s
	@mkdir -p $(dir $@)
	cd $(ROOT) && $(AS) $(ASFLAGS) -o $@ $<

$(ROOT)/$(BUILD)/%.o: $(ROOT)/%.c $(wildcard $(ROOT)/decomp/include/*.h)
	@mkdir -p $(dir $@)
	cd $(ROOT) && $(CPP) $(CPPFLAGS) $< | $(CC1) $(CC1FLAGS) -G$(call gp_of,$<) -o $@.cc1.s
	cd $(ROOT) && $(MASPSX) $(MASPSXFLAGS) -G$(call gp_of,$<) < $@.cc1.s > $@.s
	cd $(ROOT) && $(AS) $(ASFLAGS) -o $@ $@.s

$(ROOT)/$(IMAGE): $(OBJECTS) $(ROOT)/$(LINKER_SCRIPT) $(CONFIG)
	@test -n "$(OBJECTS)" || { echo "No objects: run make CONFIG=$(CONFIG) split" >&2; exit 2; }
	cd $(ROOT) && $(LD) -nostdlib --no-check-sections -Map $(IMAGE).map \
		-T $(LINKER_SCRIPT) $(addprefix -T ,$(LINKER_EXTRA)) -o $(IMAGE).elf
	cd $(ROOT) && $(OBJCOPY) -O binary $(OBJCOPY_FLAGS) $(IMAGE).elf $(IMAGE)

verify: $(ROOT)/$(IMAGE)
	cd $(ROOT) && $(PYTHON) tools/matching.py $(ORIGINAL) $(IMAGE) --sha256 $(ORIGINAL_SHA256)

coverage: $(ROOT)/$(IMAGE)
	cd $(ROOT) && $(PYTHON) tools/matching_coverage.py $(IMAGE).elf $(addprefix --src ,$(SOURCE_DIRS)) $(if $(CLASSIFICATION),--classification $(CLASSIFICATION))

clean:
	rm -rf $(ROOT)/$(BUILD) $(ROOT)/$(IMAGE) $(ROOT)/$(IMAGE).elf $(ROOT)/$(IMAGE).map
endif
