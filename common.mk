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

COPTS= -ggdb \
	-nostdlib \
	-std=c99 \
	-Os \
	-include $(NOLIBCDIR)/nolibc.h \
	-Wl,--hash-style=gnu

COPTS += -flto

ifdef UAPIDIR
	COPTS += -I$(UAPIDIR)
endif

rootskel:
	mkdir -p rootskel/sys
	mkdir -p rootskel/dev
	mkdir -p rootskel/proc
	mkdir -p rootskel/tmp
	mkdir -p rootskel/bin
	mkdir -p m68kroot/sbin
