# Explicit per-target recipes; no implicit host compiler or invented game image.
# Run inside: nix develop path:./nix/ghidra#matching
ROOT := $(abspath ..)
# The repository's tools beside this Makefile (ROOT may name another tree).
TOOLS := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../tools)
PYTHON ?= python3
AS := psx-as
LD := psx-ld
OBJCOPY := psx-objcopy
MASPSX := maspsx
.DEFAULT_GOAL := verify
.SUFFIXES:
.SECONDARY:
MAKEFLAGS += --no-builtin-rules
# Recipes run in bash with pipefail: a failing stage of a pipe (cpp into cc1,
# maspsx into the filters) fails its rule instead of leaving a partial object.
SHELL := bash
.SHELLFLAGS := -o pipefail -c
.PHONY: verify smoke split coverage audit container clean all-split all-verify all-coverage all-audit all-container
TARGET_CONFIGS := $(sort $(wildcard targets/resident/*.mk targets/overlays/*.mk))

# Every supported executable image: split, exact comparison, source coverage.
all-split all-verify all-coverage all-audit all-container:
	@for config in $(TARGET_CONFIGS); do \
		echo "== $$config"; \
		$(MAKE) --no-print-directory CONFIG=$$config $(@:all-%=%) || exit 1; \
	done

smoke:
	@command -v psx-as >/dev/null && command -v psx-ld >/dev/null && command -v psx-objcopy >/dev/null || { echo "Enter the matching Nix shell first." >&2; exit 2; }
	cd $(ROOT) && $(PYTHON) -m unittest tests.test_matching

ifeq ($(strip $(CONFIG)),)
verify split coverage audit container clean:
	@echo 'Set CONFIG=targets/<target>/<target>.mk (see docs/matching.md).' >&2
	@exit 2
else
include $(CONFIG)
$(foreach var,ORIGINAL ORIGINAL_SHA256 IMAGE LINKER_SCRIPT SPLAT_CONFIG BUILD CC_VERSION,$(if $($(var)),,$(error $(var) must be set by $(CONFIG))))

# Qualified original-compatible compilation (docs/matching.md): GCC 2.6.3/2.7.2/2.7.2-cdk cc1
# (decompals/old-gcc 0.17) for the R3000, ASPSX behaviour through maspsx 7686f845 (2.34,
# or 2.56/2.79 for the later units, which evidence only ASPSX >= 2.50), GNU binutils 2.46
# as and ld for the objects and the link.
CPPFLAGS := -undef -D__GNUC__=2 -Dmips -D__mips__ -D__mips -Dpsx -D__psx__ -D__psx \
	-D_PSYQ -D__EXTENSIONS__ -D_MIPSEL -D__CHAR_UNSIGNED__ -D_LANGUAGE_C -DLANGUAGE_C \
	-lang-c -nostdinc -I$(ROOT)/decomp/include -DORIGINAL_IMAGE='"$(ORIGINAL)"' $(TARGET_CPPFLAGS)
CC1FLAGS := -quiet -mcpu=3000 -fgnu-linker -mgas -msoft-float -O2
ASFLAGS := -EL -march=r3000 -mtune=r3000 -msoft-float -no-pad-sections -G0 \
	-I$(ROOT)/decomp/include -I$(ROOT)

# The small-data threshold and the GCC version (2.6.3, 2.7.2, 2.7.2-cdk) are properties
# of each original translation unit: GP_<file> / CC_<file> override the target's
# GP default of 0 and its CC_VERSION.
gp_of = $(or $(GP_$(basename $(notdir $(1)))),0)
cc_of = $(or $(CC_$(basename $(notdir $(1)))),$(CC_VERSION))
# CC1FLAGS_<file> adds unit-specific cc1 options, qualified per unit (no unit
# needs one at present).
cc1flags_of = $(CC1FLAGS_$(basename $(notdir $(1))))
# ASPSX behaviour (maspsx flags): 2.34 by default; MASPSX_FLAGS per target and
# MASPSX_<file> per unit (e.g. --expand-div where division checks are inline).
MASPSX_FLAGS ?= --aspsx-version=2.34
maspsx_of = $(or $(MASPSX_$(basename $(notdir $(1)))),$(MASPSX_FLAGS))
# GNU as addresses a symbol through $gp when GCC's `.extern name, size` declares it
# small, it is a small common or it lies in the unit's .sdata, also for `la`. ASPSX
# took every address (`la`) absolutely: no resident unit forms one from $gp, and the
# heap unit's small string constants, which GCC places in .sdata, are addressed with
# lui/addiu. Units with a small-data threshold therefore expand `la` themselves. Some
# units' assembler also ignored the `.extern` sizes (the sprite units, the menu
# support): EXTERN_<file> := absolute drops the directives.
LA_ABSOLUTE := -e 's/^la\t(\$$[0-9a-z]+),((\$$LC[0-9]+|[A-Za-z_][A-Za-z0-9_]*)(\+[0-9]+)?)$$/lui\t\1,%hi(\2)\naddiu\t\1,\1,%lo(\2)/'
extern_of = $(if $(filter absolute,$(EXTERN_$(basename $(notdir $(1))))),-e '/^\.extern/d')
asm_filter_of = $(if $(filter-out 0,$(call gp_of,$(1))),| sed -E $(call extern_of,$(1)) $(LA_ABSOLUTE))
# Uninitialized variables: GCC emits a unit's statics as .lcomm and its
# tentative definitions as .comm after the code, and maspsx allocates both in
# the unit's .sbss/.bss (packed without alignment; in .sbss an 8-byte object
# 8-aligned). In the original images every such object takes a slot of whole
# words, an 8-byte one also at 4 mod 8 (menu 8009265c, slot39 801ea8e4). That
# is evidenced separately for ASPSX 2.34's .lcomm statics (mdec
# 801e8958-801e8968: five u8, stored and loaded bytewise; menu3
# 80092678-800926a0; slot39 801ea710/801ea714) and for the commons PSYLINK
# allocated after every unit's own (ovl2596 801e44e0/801e44e4; libcd's
# Stsector_offset alone at 801e89bc; the world map's u16 at
# 8009bd10-8009bd1c, shared by three units), which the commons units
# reproduce. Under 2.34 and the world map's setting 2.79 (no 2.79 unit
# allocates any in C yet), this filter gives each object a whole number of
# words at a word boundary; under any other (2.56 units define only words so
# far) it rejects a smaller one, whose placement nothing evidences. The 2.79
# setting stands for the world map's assembler, which its code shows only to
# be ASPSX >= 2.50: maspsx 2.56 and 2.79 build identical objects for every
# unit set to either. Only the .sbss/.bss that maspsx appends are touched:
# any other section directive ends them.
SLOT_ASPSX := --aspsx-version=2.34 --aspsx-version=2.79
bss_filter_of = | awk -v slots=$(if $(filter $(SLOT_ASPSX),$(call maspsx_of,$(1))),1,0) \
	'$$1 ~ /^\.(section|pushsection|popsection|previous|text|data|rdata|sdata|sbss|bss)$$/ { bss = $$0 ~ /^\.section \.s?bss$$/ } \
	bss && slots && $$1 == ".align" && $$2 > 2 { $$0 = "\t.align\t2" } \
	bss && $$1 == ".space" && $$2 % 4 { if (!slots) { print "a " $$2 "-byte uninitialized variable has no slot rule under this ASPSX version" > "/dev/stderr"; exit 1 } \
	$$0 = "\t.space\t" int(($$2 + 3) / 4) * 4 } { print }'

# A probe's settings must invalidate its object, including when switching back.
# Keep the stamp's timestamp only while its contents and resolved tools agree.
shell_quote = '$(subst ','"'"',$(1))'
.PHONY: matching-flags-force
matching-flags-force:

$(ROOT)/$(BUILD)/%.cflags: matching-flags-force
	@mkdir -p $(dir $@)
	@{ printf '%s\n' \
		$(call shell_quote,CC=$(call cc_of,$*)) \
		$(call shell_quote,CPPFLAGS=$(CPPFLAGS)) \
		$(call shell_quote,CC1FLAGS=$(CC1FLAGS) $(call cc1flags_of,$*)) \
		$(call shell_quote,GP=$(call gp_of,$*)) \
		$(call shell_quote,MASPSX=$(MASPSX)) \
		$(call shell_quote,MASPSX_FLAGS=$(call maspsx_of,$*)) \
		$(call shell_quote,EXTERN=$(EXTERN_$(basename $(notdir $*)))) \
		$(call shell_quote,AS=$(AS)) \
		$(call shell_quote,ASFLAGS=$(ASFLAGS)); \
		command -v psx-cpp-$(call cc_of,$*) psx-cc1-$(call cc_of,$*) $(firstword $(MASPSX)) $(firstword $(AS)); \
	} > $@.tmp
	@cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@

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

# A C unit $(1) compiled (cpp, then $(2), then cc1), and the GAS input from its
# cc1 output $(2) (maspsx and the filters above): shared by the object and its
# coverage build.
cc1_of = psx-cpp-$(call cc_of,$(1)) $(CPPFLAGS) $(1) $(2) | psx-cc1-$(call cc_of,$(1)) $(CC1FLAGS) $(call cc1flags_of,$(1)) -G$(call gp_of,$(1))
gas_input_of = $(MASPSX) $(call maspsx_of,$(1)) -G$(call gp_of,$(1)) < $(2) $(call asm_filter_of,$(1)) $(call bss_filter_of,$(1))

$(ROOT)/$(BUILD)/%.o: $(ROOT)/%.c $(ROOT)/$(LINKER_SCRIPT) $(ROOT)/$(BUILD)/%.cflags $(MAKEFILE_LIST) $(wildcard $(ROOT)/decomp/include/*.h $(ROOT)/decomp/include/*/*.h $(ROOT)/decomp/src/*/*.h $(ROOT)/decomp/src/*/*.s)
	@mkdir -p $(dir $@)
	cd $(ROOT) && $(call cc1_of,$<) -o $@.cc1.s
	cd $(ROOT) && $(call gas_input_of,$<,$@.cc1.s) > $@.s
	cd $(ROOT) && $(AS) $(ASFLAGS) -G$(call gp_of,$<) -o $@ $@.s

# Source coverage attributes every byte of a C unit's object by where GAS put
# it (tools/matching_coverage.py): the coverage build compiles the unit again
# with a label line at both ends of each asm statement's text and assembles it
# with each label recording its position in every section and the GAS macro
# expansions so far. The report checks that without the labels the cc1
# output, the GAS input and the sections are the object's own.
$(ROOT)/$(BUILD)/%.cov.o: $(ROOT)/%.c $(ROOT)/$(BUILD)/%.o $(TOOLS)/matching_coverage.py
	cd $(ROOT) && $(call cc1_of,$<,| $(PYTHON) $(TOOLS)/matching_coverage.py --mark-asm) -o $@.cc1.s
	cd $(ROOT) && $(call gas_input_of,$<,$@.cc1.s) > $@.s
	cd $(ROOT) && $(PYTHON) $(TOOLS)/matching_coverage.py --mark-gas < $@.s > $@.marks.s
	cd $(ROOT) && $(AS) $(ASFLAGS) -G$(call gp_of,$<) -o $@ $@.marks.s

$(ROOT)/$(IMAGE): $(OBJECTS) $(ROOT)/$(LINKER_SCRIPT) $(wildcard $(addprefix $(ROOT)/,$(LINKER_EXTRA))) $(CONFIG)
	@test -n "$(OBJECTS)" || { echo "No objects: run make CONFIG=$(CONFIG) split" >&2; exit 2; }
	cd $(ROOT) && $(LD) -nostdlib --no-check-sections -Map $(IMAGE).map \
		-T $(LINKER_SCRIPT) $(addprefix -T ,$(LINKER_EXTRA)) -o $(IMAGE).elf
	cd $(ROOT) && $(OBJCOPY) -O binary $(OBJCOPY_FLAGS) $(IMAGE).elf $(IMAGE)
	$(if $(PACKER_TAIL),cd $(ROOT) && head -c $(PACKER_TAIL) /dev/zero >> $(IMAGE))

verify: $(ROOT)/$(IMAGE)
	cd $(ROOT) && $(PYTHON) tools/matching.py $(ORIGINAL) $(IMAGE) --sha256 $(ORIGINAL_SHA256)

# The coverage builds of the C units among the objects.
COVERAGE_OBJECTS = $(foreach object,$(OBJECTS),$(if $(wildcard $(patsubst $(ROOT)/$(BUILD)/%.o,$(ROOT)/%.c,$(object))),$(object:.o=.cov.o)))

coverage: $(ROOT)/$(IMAGE) $(COVERAGE_OBJECTS)
	cd $(ROOT) && $(PYTHON) tools/matching_coverage.py $(IMAGE).elf --map $(IMAGE).map $(addprefix --src ,$(SOURCE_DIRS)) $(if $(CLASSIFICATION),--classification $(CLASSIFICATION))

audit: coverage
	cd $(ROOT) && $(PYTHON) tools/nonmatching_score.py $(abspath $(CONFIG)) --exact

# Compressed-container reproduction (a separate claim from image matching):
# CONTAINERS lists DISC:SLOT pairs whose packed file holds this image.
# PACKER_TAIL counts the zero literals the original packer appended to complete
# its last group; the image rule appends them after the linked program, so the
# image matches only when the link ends exactly where the program does.
container: $(ROOT)/$(IMAGE)
	cd $(ROOT) && for pair in $(CONTAINERS); do \
		disc=$${pair%%:*}; slot=$${pair##*:}; \
		$(PYTHON) tools/packed_container.py $(IMAGE) --tail $(or $(PACKER_TAIL),0) \
			--disc .local/discs/disc$$disc.bin \
			--manifest .local/extract/disc$$disc/manifest.json --slot $$slot || exit 1; \
	done

clean:
	rm -rf $(ROOT)/$(BUILD) $(ROOT)/$(IMAGE) $(ROOT)/$(IMAGE).elf $(ROOT)/$(IMAGE).map
endif
