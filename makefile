# =====================================================================
#  Operating System Project - master makefile
#
#  src/            shared kernel sources (the current / master kernel)
#  tests/<name>/   one directory per test kernel:
#                    test.mk   - DESC, OBJS to link, optional DISK image
#                    kernel.C  - the test's main()
#                    *.C/*.H/* - any files that replace the ones in src/
#
#  For a given TEST, src/ and tests/$(TEST)/ are merged into
#  build/$(TEST)/src (test files win), compiled into build/$(TEST)/,
#  and linked into build/$(TEST)/kernel.bin.
#
#  Usage:  make [TEST=file-system] [all|run|debug]   or   make scheduler / run-scheduler / debug-scheduler
#          make list                          show available tests
# =====================================================================

AS = nasm
PLATFORM := $(shell uname)

ifeq ($(PLATFORM), Linux)
  GCC ?= x86_64-linux-gnu-gcc
  ifeq ($(origin LD), default)
    LD := $(if $(shell command -v x86_64-linux-gnu-ld),x86_64-linux-gnu-ld,ld)
  endif
endif

ifeq ($(PLATFORM), Darwin)
  GCC ?= x86_64-elf-gcc
  ifeq ($(origin LD), default)
    LD := x86_64-elf-ld
  endif
endif

QEMU = qemu-system-x86_64

# ---- test selection --------------------------------------------------

TESTS := $(sort $(patsubst tests/%/test.mk,%,$(wildcard tests/*/test.mk)))
TEST  ?= file-system

ifeq ($(filter $(TEST),$(TESTS)),)
  $(error Unknown TEST '$(TEST)'. Available tests: $(TESTS))
endif

DESC :=
DISK :=
OBJS :=
include tests/$(TEST)/test.mk

SRC   = src
TDIR  = tests/$(TEST)
BUILD = build/$(TEST)
STAGE = $(BUILD)/src

KERNEL := $(BUILD)/kernel.bin
OBJS   := $(addprefix $(BUILD)/, $(OBJS))

# Every source file the test can see (test overrides + shared sources).
SOURCES := $(sort $(notdir $(wildcard $(SRC)/*.C $(SRC)/*.H $(SRC)/*.asm $(SRC)/*.ld \
                                      $(TDIR)/*.C $(TDIR)/*.H $(TDIR)/*.asm $(TDIR)/*.ld)))
STAGED  := $(addprefix $(STAGE)/, $(SOURCES))

GCC_OPTIONS = -m32 -nostdlib -fno-builtin -nostartfiles -nodefaultlibs -fno-exceptions \
   -fno-rtti -fno-stack-protector -fleading-underscore -fno-asynchronous-unwind-tables \
   -fno-pie -I$(STAGE) -MMD -MP

ifneq ($(DISK),)
  QEMU_DISK = -device piix3-ide,id=ide -drive id=disk,file=$(TDIR)/$(DISK),format=raw,if=none \
              -device ide-hd,drive=disk,bus=ide.0
endif

# ---- top-level targets -----------------------------------------------

all: $(KERNEL)
	@echo "Built test '$(TEST)' -> $(KERNEL)"

run: $(KERNEL)
	$(QEMU) -kernel $(KERNEL) -serial stdio $(QEMU_DISK)

debug: $(KERNEL)
	$(QEMU) -s -S -kernel $(KERNEL) $(QEMU_DISK)

list:
	@$(foreach t,$(TESTS),printf '  %-12s %s\n' '$(t)' "$$(sed -n 's/^DESC *= *//p' tests/$(t)/test.mk)";)

# Build every test kernel (handy sanity check after changing src/).
all-tests:
	@set -e; $(foreach t,$(TESTS),$(MAKE) --no-print-directory TEST=$(t) all;)

clean:
	rm -rf build kernel.bin

# Shorthands: `make scheduler`, `make run-scheduler`, `make debug-scheduler`
$(TESTS):
	@$(MAKE) --no-print-directory TEST=$@ all

$(addprefix run-,$(TESTS)):
	@$(MAKE) --no-print-directory TEST=$(@:run-%=%) run

$(addprefix debug-,$(TESTS)):
	@$(MAKE) --no-print-directory TEST=$(@:debug-%=%) debug

.PHONY: all run debug list all-tests clean $(TESTS) \
        $(addprefix run-,$(TESTS)) $(addprefix debug-,$(TESTS))

# ---- staging: merge src/ and tests/$(TEST)/ ---------------------------
# Both rules match; make prefers the first one whose prerequisite exists,
# so a file in the test directory overrides the one in src/.

$(STAGE):
	mkdir -p $@

$(STAGE)/%: $(TDIR)/% | $(STAGE)
	cp $< $@

$(STAGE)/%: $(SRC)/% | $(STAGE)
	cp $< $@

# ---- compile & link --------------------------------------------------

$(BUILD)/%.o: $(STAGE)/%.C | $(STAGED)
	$(GCC) $(GCC_OPTIONS) -c -o $@ $<

$(BUILD)/%.o: $(STAGE)/%.asm | $(STAGED)
	$(AS) -f elf -i $(STAGE)/ -MD $(@:.o=.d) -MP -o $@ $<

$(KERNEL): $(OBJS) $(STAGE)/linker.ld
	$(LD) -melf_i386 -T $(STAGE)/linker.ld -o $@ $(OBJS)

-include $(OBJS:.o=.d)
