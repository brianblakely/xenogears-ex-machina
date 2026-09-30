# Explicit per-target recipes; no implicit host compiler or invented game image.
# Run inside: nix develop path:./nix/ghidra#matching
ROOT := $(abspath ..)
PYTHON ?= python3
AS := psx-as
LD := psx-ld
OBJCOPY := psx-objcopy
MASPSX := maspsx
.DEFAULT_GOAL := verify
.SUFFIXES:
.SECONDARY:
MAKEFLAGS += --no-builtin-rules
.PHONY: verify smoke split coverage container clean all-split all-verify all-coverage all-container
TARGET_CONFIGS := $(sort $(wildcard targets/resident/*.mk targets/overlays/*.mk))

# Every supported executable image: split, exact comparison, source coverage.
all-split all-verify all-coverage all-container:
	@for config in $(TARGET_CONFIGS); do \
		echo "== $$config"; \
		$(MAKE) --no-print-directory CONFIG=$$config $(@:all-%=%) || exit 1; \
	done

smoke:
	@command -v psx-as >/dev/null && command -v psx-ld >/dev/null && command -v psx-objcopy >/dev/null || { echo "Enter the matching Nix shell first." >&2; exit 2; }
	cd $(ROOT) && $(PYTHON) -m unittest tests.test_matching

ifeq ($(strip $(CONFIG)),)
verify split coverage container clean:
	@echo 'Set CONFIG=targets/<target>/<target>.mk (see docs/matching.md).' >&2
	@exit 2
else
include $(CONFIG)
$(foreach var,ORIGINAL ORIGINAL_SHA256 IMAGE LINKER_SCRIPT SPLAT_CONFIG BUILD CC_VERSION,$(if $($(var)),,$(error $(var) must be set by $(CONFIG))))

# Qualified original-compatible compilation (docs/matching.md): GCC 2.6.3/2.7.2 cc1
# for the R3000, ASPSX 2.34 behaviour through maspsx, GNU as for the object.
CPPFLAGS := -undef -D__GNUC__=2 -Dmips -D__mips__ -D__mips -Dpsx -D__psx__ -D__psx \
	-D_PSYQ -D__EXTENSIONS__ -D_MIPSEL -D__CHAR_UNSIGNED__ -D_LANGUAGE_C -DLANGUAGE_C \
	-lang-c -nostdinc -I$(ROOT)/decomp/include $(TARGET_CPPFLAGS)
CC1FLAGS := -quiet -mcpu=3000 -fgnu-linker -mgas -msoft-float -O2
ASFLAGS := -EL -march=r3000 -mtune=r3000 -msoft-float -no-pad-sections -G0 \
	-I$(ROOT)/decomp/include -I$(ROOT)

# The small-data threshold and the GCC version (2.6.3 or 2.7.2) are properties
# of each original translation unit: GP_<file> / CC_<file> override the target's
# GP default of 0 and its CC_VERSION.
gp_of = $(or $(GP_$(basename $(notdir $(1)))),0)
cc_of = $(or $(CC_$(basename $(notdir $(1)))),$(CC_VERSION))
# ASPSX behaviour (maspsx flags): 2.34 by default; MASPSX_FLAGS per target and
# MASPSX_<file> per unit (e.g. --expand-div where division checks are inline).
MASPSX_FLAGS ?= --aspsx-version=2.34
maspsx_of = $(or $(MASPSX_$(basename $(notdir $(1)))),$(MASPSX_FLAGS))

# Objects come from the splat-generated linker script (regenerate with `split`).
LP := (
OBJECTS = $(addprefix $(ROOT)/,$(shell sed -n 's/^ *\([^ ]*\.o\)[$(LP)].*/\1/p' $(ROOT)/$(LINKER_SCRIPT) 2>/dev/null | sort -u))

# A fresh split: splat's cache would otherwise keep assembly with stale symbol names.
# SPLIT_ALSO names configurations sharing this target's assembly directory.
split:
	rm -rf $(ROOT)/$(dir $(LINKER_SCRIPT))
	cd $(ROOT) && splat split $(SPLAT_CONFIG)
	cd $(ROOT) && for also in $(SPLIT_ALSO); do \
		rm -rf "$$(dirname "$$(sed -n 's/^ *ld_script_path: *//p' $$also)")" && splat split $$also || exit 1; \
	done

# Objects are rebuilt after every split (they include the generated assembly).
$(ROOT)/$(BUILD)/%.o: $(ROOT)/%.s $(ROOT)/$(LINKER_SCRIPT)
	@mkdir -p $(dir $@)
	cd $(ROOT) && $(AS) $(ASFLAGS) -o $@ $<

$(ROOT)/$(BUILD)/%.o: $(ROOT)/%.c $(ROOT)/$(LINKER_SCRIPT) $(wildcard $(ROOT)/decomp/include/*.h $(ROOT)/decomp/include/*/*.h $(dir $(ROOT)/%)*.h)
	@mkdir -p $(dir $@)
	cd $(ROOT) && psx-cpp-$(call cc_of,$<) $(CPPFLAGS) $< | psx-cc1-$(call cc_of,$<) $(CC1FLAGS) -G$(call gp_of,$<) -o $@.cc1.s
	cd $(ROOT) && $(MASPSX) $(call maspsx_of,$<) -G$(call gp_of,$<) < $@.cc1.s > $@.s
	cd $(ROOT) && $(AS) $(ASFLAGS) -G$(call gp_of,$<) -o $@ $@.s

$(ROOT)/$(IMAGE): $(OBJECTS) $(ROOT)/$(LINKER_SCRIPT) $(CONFIG)
	@test -n "$(OBJECTS)" || { echo "No objects: run make CONFIG=$(CONFIG) split" >&2; exit 2; }
	cd $(ROOT) && $(LD) -nostdlib --no-check-sections -Map $(IMAGE).map \
		-T $(LINKER_SCRIPT) $(addprefix -T ,$(LINKER_EXTRA)) -o $(IMAGE).elf
	cd $(ROOT) && $(OBJCOPY) -O binary $(OBJCOPY_FLAGS) $(IMAGE).elf $(IMAGE)

verify: $(ROOT)/$(IMAGE)
	cd $(ROOT) && $(PYTHON) tools/matching.py $(ORIGINAL) $(IMAGE) --sha256 $(ORIGINAL_SHA256)

coverage: $(ROOT)/$(IMAGE)
	cd $(ROOT) && $(PYTHON) tools/matching_coverage.py $(IMAGE).elf $(addprefix --src ,$(SOURCE_DIRS)) $(if $(CLASSIFICATION),--classification $(CLASSIFICATION))

# Compressed-container reproduction (a separate claim from image matching):
# CONTAINERS lists DISC:SLOT pairs whose packed file holds this image.
container: $(ROOT)/$(IMAGE)
	cd $(ROOT) && for pair in $(CONTAINERS); do \
		disc=$${pair%%:*}; slot=$${pair##*:}; \
		$(PYTHON) tools/packed_container.py $(IMAGE) --disc .local/discs/disc$$disc.bin \
			--manifest .local/extract/disc$$disc/manifest.json --slot $$slot || exit 1; \
	done

clean:
	rm -rf $(ROOT)/$(BUILD) $(ROOT)/$(IMAGE) $(ROOT)/$(IMAGE).elf $(ROOT)/$(IMAGE).map
endif
