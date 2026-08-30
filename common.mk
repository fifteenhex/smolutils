MAKEFLAGS += --no-builtin-rules

# Make sure we know where to get nolibc
ifndef NOLIBCDIR
$(error Please pass NOLIBCDIR with the path to your copy of nolibc (tools/include/nolibc/ in the linux source))
endif

# Make sure we know where the toolchain is
ifndef CROSS_COMPILE
$(error Please pass CROSS_COMPILE with the prefix of you toolchain)
endif

# Make sure we know where tarwak is
ifndef TARWAK
$(error Please pass TARWAK with the path of your tarwak binary)
endif

CC=$(CROSS_COMPILE)gcc
BFDLD=$(CROSS_COMPILE)ld.bfd
STRIP=$(CROSS_COMPILE)strip

PROGS_SYSTEM = init	\
	       getty	\
	       insmod	\
	       startup

PROGS_USER =		\
	smolsh		\
	dmesg		\
	ls		\
	ps		\
	cat		\
	sha256sum	\
	xxd		\
	man		\
	less		\
	uname		\
	cp		\
	touch		\
	chmod		\
	chown		\
	kill		\
	df		\
	su		\
	mount

# Make some warnings into errors because I am bad at the programming
_COPTS =  -Werror=return-type
_COPTS += -Werror=implicit-function-declaration
_COPTS += -flto
_COPTS += -ggdb -nostdlib -std=c99 -Os

# Feature parsing

fempty :=
fspace := $(empty) $(empty)
fcomma := ,

DISABLE_LIST := $(subst $(fcomma),$(fspace),$(FEATURE_DISABLE))

ifneq ($(filter net,$(DISABLE_LIST)),)
_COPTS += -DCONFIG_NETWORK=n
else
PROGS_NET_SYSTEM =	\
	sntp		\
	dhcpc

PROGS_NET_USER =	\
	ping		\
	resolv		\
	tftp
_TARWAKFEATURES += -fnet
endif

ifneq ($(filter initramfs,$(DISABLE_LIST)),)
_COPTS += -DCONFIG_INITRAMFS=n
else
_TARWAKFEATURES += -finitramfs
endif

ifneq ($(filter modules,$(DISABLE_LIST)),)
_COPTS += -DCONFIG_MODULES=n
else
_TARWAKFEATURES += -fmodules
endif

COPTS= -include $(NOLIBCDIR)/nolibc.h \
	-Wl,--hash-style=gnu \
	$(_COPTS)

C_FILES = $(addsuffix .c,$(PROGS_SYSTEM)) $(addsuffix .c,$(PROGS_USER)) \
	  $(addsuffix .c,$(PROGS_NET_SYSTEM)) $(addsuffix .c,$(PROGS_NET_USER))

# UAPIDIR may be a space separated list of directories
ifdef UAPIDIR
	COPTS += $(addprefix -I,$(UAPIDIR))
endif

HEADERS = config.h \
	  common.h \
	  users.h \
	  net.h \
	  resolv.h \
	  memfd.h \
	  multicall.h \
	  nolibc_extensions/unistd.h \
	  nolibc_extensions/signal.h

EROFS_CMD = mkfs.erofs -E force-inode-compact,all-fragments,dedupe -zlz4hc --tar
