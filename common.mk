include smolcommon.mk

# Make sure we know where tarwak is
ifndef TARWAK
$(error Please pass TARWAK with the path of your tarwak binary)
endif

PROGS_SYSTEM = init	\
	       getty	\
	       modules	\
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
	man		\
	less		\
	uname		\
	df		\
	su		\
	mount

# Feature parsing

fempty :=
fspace := $(empty) $(empty)
fcomma := ,

DISABLE_LIST := $(subst $(fcomma),$(fspace),$(FEATURE_DISABLE))

ifneq ($(filter net,$(DISABLE_LIST)),)
_COPTS += -DCONFIG_NETWORK=n
_COPTS += -DCONFIG_TELNETD=n
else
PROGS_NET_SYSTEM =	\
	sntp		\
	dhcpc

PROGS_NET_USER =	\
	ip		\
	ping		\
	resolv		\
	tftp
_TARWAKFEATURES += -fnet

ifneq ($(filter telnetd,$(DISABLE_LIST)),)
_COPTS += -DCONFIG_TELNETD=n
else
PROGS_NET_SYSTEM += telnetd
_TARWAKFEATURES += -ftelnetd
endif
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

C_FILES = $(addsuffix .c,$(PROGS_SYSTEM)) $(addsuffix .c,$(PROGS_USER)) \
	  $(addsuffix .c,$(PROGS_NET_SYSTEM)) $(addsuffix .c,$(PROGS_NET_USER))

HEADERS = config.h \
	  auth.h \
	  common.h \
	  cmdline.h \
	  users.h \
	  seat.h \
	  net.h \
	  readln.h \
	  resolv.h \
	  colour.h \
	  sysfs.h \
	  dhcpc.h \
	  later.h \
	  memfd.h \
	  multicall.h

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
