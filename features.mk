# "Features", sets of binaries that can be dropped completely to save space.
# Pass FEATURE_DISABLE=net,mtd,.. to leave them out.

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
PROGS_SYSTEM += modules
_TARWAKFEATURES += -fmodules
endif

ifneq ($(filter minions,$(DISABLE_LIST)),)
_COPTS += -DCONFIG_MINIONS=n
endif

ifneq ($(filter mtd,$(DISABLE_LIST)),)
_COPTS += -DCONFIG_MTD=n
else
PROGS_USER += mtd
_TARWAKFEATURES += -fmtd
endif

ifneq ($(filter gpio,$(DISABLE_LIST)),)
_COPTS += -DCONFIG_GPIO=n
else
PROGS_USER += gpio
_TARWAKFEATURES += -fgpio
endif

ifneq ($(filter keeper,$(DISABLE_LIST)),)
_COPTS += -DCONFIG_KEEPER=n
else
PROGS_SYSTEM += keeper
_TARWAKFEATURES += -fkeeper
endif
