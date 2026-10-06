# Rules for building binaries in the *smol* way.

MAKEFLAGS += --no-builtin-rules

# stop deleting my dbg elf!
.SECONDARY:

# So HEADERS is read when the rule run because we extend it later.
.SECONDEXPANSION:

# Reduce the output, pass V=1 to get the noise back
ifeq ($(V),1)
Q =
MSG = @:
else
Q = @
MSG = @printf '  %-7s %s\n'
endif

ifdef SMOL_ARCH

ifndef NOLIBCDIR
$(error Please pass NOLIBCDIR with the path to your copy of nolibc (tools/include/nolibc/ in the linux source))
endif

ifndef NOLIBCEXTDIR
$(error Please pass NOLIBCEXTDIR with the path to your clone of nolibc-extensions)
endif

ifndef CROSS_COMPILE
$(error Please pass CROSS_COMPILE with the prefix of you toolchain)
endif

CC=$(CROSS_COMPILE)gcc
BFDLD=$(CROSS_COMPILE)ld.bfd
STRIP=$(CROSS_COMPILE)strip

# Make some warnings into errors because I am bad at the programming
_COPTS =  -Werror=return-type
_COPTS += -Werror=implicit-function-declaration
_COPTS += -flto
_COPTS += -ggdb -nostdlib -std=c99 -Os

COPTS= -include $(NOLIBCDIR)/nolibc.h \
	-include $(NOLIBCEXTDIR)/include/nolibc-extensions.h \
	-Wl,--hash-style=gnu \
	$(_COPTS)

# UAPIDIR may be a space separated list of directories
ifdef UAPIDIR
	COPTS += $(addprefix -I,$(UAPIDIR))
endif

SMOL_SUFFIX = $(SMOL_ARCH)

ifeq ($(SMOL_ARCH),x86_64)
COPTS += -D R_AMD64_RELATIVE=8 -Wl,-z,noseparate-code
_PIE = -fpie -static-pie
_LINK = pie
else ifneq ($(filter $(SMOL_ARCH),cortexa7 cortexa9),)
COPTS += -march=armv7-a -mtune=$(subst cortexa,cortex-a,$(SMOL_ARCH)) \
	 -D R_ARM_RELATIVE=23
COPTS += -Wl,-u,raise
SMOL_LIBS += -lgcc
_PIE = -fpie -static-pie
_LINK = static
else ifeq ($(SMOL_ARCH),68000)
COPTS += -m68000 -mstrict-align -D R_68K_RELATIVE=22
# Segments with different permissions can share a page when there's no MMU
COPTS += -Wl,-z,max-page-size=4
_PIE = -fpie -pie -Wl,--no-dynamic-linker
_LINK = pie
else ifneq ($(filter $(SMOL_ARCH),68030 68040 68060),)
COPTS += -m$(SMOL_ARCH) -D R_68K_RELATIVE=22
COPTS += -Wl,-z,max-page-size=4096
SMOL_LIBS += -lgcc
_PIE = -fpie -pie -Wl,--no-dynamic-linker
_LINK = pie
else ifeq ($(SMOL_ARCH),sh4)
COPTS += -m4 -D R_SH_RELATIVE=165
# LTO fix
COPTS += -Wl,-u,_start_wrapper
SMOL_LIBS += -lgcc
_PIE = -fpie -pie -Wl,--no-dynamic-linker
_LINK = static
else
$(error SMOL_ARCH=$(SMOL_ARCH) is not one of x86_64 cortexa7 cortexa9 68000 68030 68040 68060 sh4)
endif

ifdef PIE
_LINK = pie
endif

ifdef NOPIE
_LINK = static
endif

ifeq ($(_LINK),pie)
STATICPIE ?= $(_PIE)
else
STATICPIE ?= -static
endif

SMOL_BUILDMODE = .buildmode.$(SMOL_SUFFIX)

.PHONY: FORCE
$(SMOL_BUILDMODE): FORCE
	$(Q)echo '$(STATICPIE)' | cmp -s - $@ 2>/dev/null || echo '$(STATICPIE)' > $@

%.$(SMOL_SUFFIX).elf.dbg: %.c $$(HEADERS) $(SMOL_BUILDMODE)
	$(MSG) CC $*
	$(Q)$(CC) $(COPTS) \
	$(STATICPIE) \
	-o $@ $< $(SMOL_LIBS)

%.$(SMOL_SUFFIX).elf: %.$(SMOL_SUFFIX).elf.dbg
	$(MSG) STRIP $*
	$(Q)$(STRIP) $< -o $@

endif	# SMOL_ARCH

HOSTCC ?= gcc
HOSTSTRIP ?= strip

LIBC_COPTS ?= -ggdb -std=gnu99 -Os
LIBC_WOPTS ?= -Wall -Wextra

%.libc.elf.dbg: %.c $$(HEADERS)
	$(MSG) HOSTCC $*
	$(Q)$(HOSTCC) $(LIBC_COPTS) $(LIBC_WOPTS) -o $@ $< $(LIBC_LIBS)

%.libc.elf: %.libc.elf.dbg
	$(MSG) STRIP $*
	$(Q)$(HOSTSTRIP) $< -o $@

# Formatting, to the style in .clang-format next to this. Only what's at the
# top level: anything vendored under a subdirectory is left as it came.
CLANG_FORMAT ?= clang-format
SMOL_FORMAT_FILES ?= $(wildcard *.c *.h)

.PHONY: clang-format
clang-format:
	$(MSG) FORMAT "$(words $(SMOL_FORMAT_FILES)) files"
	$(Q)$(CLANG_FORMAT) -i $(SMOL_FORMAT_FILES)

# Says what it would change without changing it, and fails if there is any
.PHONY: clang-format-check
clang-format-check:
	$(Q)$(CLANG_FORMAT) --dry-run -Werror $(SMOL_FORMAT_FILES)
