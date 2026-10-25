# "Features", sets of binaries that can be dropped completely to save space.
# Pass FEATURE_DISABLE=net,mtd,.. to leave them out.

# Feature parsing

fempty :=
fspace := $(empty) $(empty)
fcomma := ,

DISABLE_LIST := $(subst $(fcomma),$(fspace),$(FEATURE_DISABLE))

ifeq ($(filter net,$(DISABLE_LIST)),)
PROGS_NET_SYSTEM =	\
	sntp		\
	dhcpc

PROGS_NET_USER =	\
	ip		\
	ping		\
	resolv		\
	tftp
_TARWAKFEATURES += -fnet

ifeq ($(filter telnetd,$(DISABLE_LIST)),)
PROGS_NET_SYSTEM += telnetd
_TARWAKFEATURES += -ftelnetd
endif
endif

ifeq ($(filter initramfs,$(DISABLE_LIST)),)
_TARWAKFEATURES += -finitramfs
endif

ifeq ($(filter modules,$(DISABLE_LIST)),)
PROGS_SYSTEM += modules
_TARWAKFEATURES += -fmodules
endif

ifeq ($(filter mtd,$(DISABLE_LIST)),)
PROGS_USER += mtd
_TARWAKFEATURES += -fmtd
endif

ifeq ($(filter gpio,$(DISABLE_LIST)),)
PROGS_USER += gpio
_TARWAKFEATURES += -fgpio
endif

ifeq ($(filter keeper,$(DISABLE_LIST)),)
PROGS_SYSTEM += keeper
_TARWAKFEATURES += -fkeeper
endif
