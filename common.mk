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

PROGS = init		\
	getty		\
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
	uname

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
