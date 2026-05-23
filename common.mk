MAKEFLAGS += --no-builtin-rules

# Make sure we know where to get nolibc
ifndef NOLIBCDIR
$(error Please pass NOLIBCDIR with the path to your copy of nolibc (tools/include/nolibc/ in the linux source))
endif

# Make sure we know where the toolchain is
ifndef CROSS_COMPILE
$(error Please pass CROSS_COMPILE with the prefix of you toolchain)
endif

CC=$(CROSS_COMPILE)gcc
BFDLD=$(CROSS_COMPILE)ld.bfd
STRIP=$(CROSS_COMPILE)strip

PROGS_SYSTEM = init	\
	       getty	\
	       startup

PROGS_USER =		\
	smolsh		\
	dmesg		\
	ls		\
	ps		\
	cat		\
	mkdir		\
	sha256sum	\
	xxd		\
	man		\
	less		\
	uname		\
	cp		\
	mv		\
	touch		\
	ln		\
	chmod		\
	chown		\
	kill		\
	df		\
	mount		\
	umount

PROGS_NET_SYSTEM =	\
	sntp		\
	dhcpc

PROGS_NET_USER =	\
	ping		\
	resolv

# Make some warnings into errors because I am bad at the programming
_COPTS = -Werror=return-type
_COPTS += -flto
_COPTS += -ggdb -nostdlib -std=c99 -Os

COPTS= -include $(NOLIBCDIR)/nolibc.h \
	-Wl,--hash-style=gnu \
	$(_COPTS)

C_FILES = $(addsuffix .c,$(PROGS_SYSTEM)) $(addsuffix .c,$(PROGS_USER))

ifdef UAPIDIR
	COPTS += -I$(UAPIDIR)
endif

HEADERS = common.h \
	  users.h \
	  net.h \
	  resolv.h \
	  memfd.h

rootskel:
	mkdir -p rootskel/sys
	mkdir -p rootskel/dev
	mkdir -p rootskel/proc
	mkdir -p rootskel/tmp
	mkdir -p rootskel/bin
	mkdir -p m68kroot/sbin

EROFS_CMD = mkfs.erofs -E force-inode-compact,all-fragments,dedupe -zlz4hc --tar
