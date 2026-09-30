# Explicit per-target recipes; no implicit host compiler or invented game image.
ROOT := $(abspath ..)
PYTHON ?= python3
AS := psx-as
LD := psx-ld
OBJCOPY := psx-objcopy
.DEFAULT_GOAL := verify
.PHONY: verify smoke

smoke:
	cd $(ROOT) && $(PYTHON) -m unittest tests.test_matching

ifeq ($(strip $(CONFIG)),)
verify:
	@echo 'Set CONFIG=targets/<target>/target.mk after qualifying the original toolchain.' >&2
	@exit 2
else
include $(CONFIG)
$(foreach var,ORIGINAL ORIGINAL_SHA256 IMAGE OBJECTS LINKER_SCRIPT,$(if $($(var)),,$(error $(var) must be set by $(CONFIG))))

$(IMAGE): $(OBJECTS) $(LINKER_SCRIPT)
	mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -T $(LINKER_SCRIPT) -Map $@.map -o $@.elf $(OBJECTS)
	$(OBJCOPY) -O binary $@.elf $@

verify: $(IMAGE)
	$(PYTHON) $(ROOT)/tools/matching.py $(ORIGINAL) $(IMAGE) --sha256 $(ORIGINAL_SHA256)
endif
