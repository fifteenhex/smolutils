include smolcommon.mk

# Make sure we know where tarwak is
ifndef TARWAK
$(error Please pass TARWAK with the path of your tarwak binary)
endif

PROGS_SYSTEM = init	\
	       getty	\
	       blkwak	\
	       startup

PROGS_USER =		\
	smolsh		\
	dmesg		\
	ls		\
	process		\
	files		\
	sha256sum	\
	xxd		\
	devmem		\
	lsbus		\
	rtc		\
	i2c		\
	man		\
	less		\
	uname		\
	df		\
	su		\
	mount

include features.mk

# Include config.h.local first so it overrides the defaults
LOCAL_CONFIG = $(wildcard config.h.local)
ifneq ($(LOCAL_CONFIG),)
_COPTS += -include $(LOCAL_CONFIG)
endif

# Like .buildmode, a stamp for if config.h.local exists so adding or
# removing it rebuilds everything
LOCAL_CONFIG_STAMP = .localconfig.$(SMOL_SUFFIX)
$(LOCAL_CONFIG_STAMP): FORCE
	$(Q)echo '$(LOCAL_CONFIG)' | cmp -s - $@ 2>/dev/null || echo '$(LOCAL_CONFIG)' > $@

C_FILES = $(addsuffix .c,$(PROGS_SYSTEM)) $(addsuffix .c,$(PROGS_USER)) \
	  $(addsuffix .c,$(PROGS_NET_SYSTEM)) $(addsuffix .c,$(PROGS_NET_USER))

HEADERS = $(LOCAL_CONFIG) $(LOCAL_CONFIG_STAMP) \
	  config.h \
	  common.h \
	  seat.h \
	  dhcpc.h \
	  later.h \
	  dungeon.h \
	  inc/android_sparse_img.h \
	  inc/asserts.h \
	  inc/auth.h \
	  inc/cleanup.h \
	  inc/cmdline.h \
	  inc/colour.h \
	  inc/date.h \
	  inc/fs.h \
	  inc/gpio.h \
	  inc/io.h \
	  inc/kernfs.h \
	  inc/memfd.h \
	  inc/misc.h \
	  inc/mmap.h \
	  inc/multicall.h \
	  inc/net.h \
	  inc/numbers.h \
	  inc/paths.h \
	  inc/printing.h \
	  inc/pts.h \
	  inc/readln.h \
	  inc/resolv.h \
	  inc/signals.h \
	  inc/spawn.h \
	  inc/strings.h \
	  inc/sysfs.h \
	  inc/time.h \
	  inc/tool.h \
	  inc/unaligned.h \
	  inc/users.h \
	  inc/util.h

# This allows you to extend the rootfs with tarballs containing extra
# goodies but the configuration from rootfs.tarwak.json applies, probably
# in interesting/unexpected ways,..
ifdef EXTRA_TARS
ifndef TARMUNGE
$(error Please pass TARMUNGE with the path of your tarmunge binary to use EXTRA_TARS)
endif
endif

# $(1) is the tarball to make, $(2) the pattern tarwak names elfs by
define build_tar
	$(MSG) TARWAK $(1)
	$(Q)$(TARWAK) -i rootfs.tarwak.json -o $(1) -b ./ -p "$(2)" $(_TARWAKFEATURES)
	$(if $(EXTRA_TARS),$(MSG) TARMUNGE $(1))
	$(if $(EXTRA_TARS),$(Q)$(TARMUNGE) -i rootfs.tarwak.json -o $(1).tmp $(1) $(EXTRA_TARS))
	$(if $(EXTRA_TARS),$(Q)mv $(1).tmp $(1))
endef

EROFS_CMD = mkfs.erofs -E force-inode-compact,all-fragments,dedupe -zlz4hc --tar
