# Explicit per-target recipes; no implicit host compiler or invented game image.
ROOT := $(abspath ..)
PYTHON ?= python3
AS := psx-as
LD := psx-ld
OBJCOPY := psx-objcopy
.DEFAULT_GOAL := verify
.SUFFIXES:
MAKEFLAGS += --no-builtin-rules
.PHONY: verify smoke

smoke:
	@command -v psx-as >/dev/null && command -v psx-ld >/dev/null && command -v psx-objcopy >/dev/null || { echo "Enter the matching Nix shell first." >&2; exit 2; }
	cd $(ROOT) && $(PYTHON) -m unittest tests.test_matching

ifeq ($(strip $(CONFIG)),)
verify:
	@echo 'Set CONFIG=targets/<target>/target.mk after qualifying the original toolchain.' >&2
	@exit 2
else
include $(CONFIG)
$(foreach var,ORIGINAL ORIGINAL_SHA256 IMAGE OBJECTS LINKER_SCRIPT,$(if $($(var)),,$(error $(var) must be set by $(CONFIG))))

$(IMAGE): $(OBJECTS) $(LINKER_SCRIPT) $(CONFIG)
	mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -T $(LINKER_SCRIPT) -Map $@.map -o $@.elf $(OBJECTS)
	$(OBJCOPY) -O binary $@.elf $@

verify: $(IMAGE)
	$(PYTHON) $(ROOT)/tools/matching.py $(ORIGINAL) $(IMAGE) --sha256 $(ORIGINAL_SHA256)
endif
