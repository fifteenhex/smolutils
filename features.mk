# "Features" - Like BSD sets or something
#
# Pass features to select what you want, look at FEATURES_ALL for what you can have.

fempty :=
fspace := $(empty) $(empty)
fcomma := ,

FEATURES_ALL = net telnetd initramfs modules mtd i2c gpio keeper
FEATURES ?= $(FEATURES_ALL)

# Internalise FEATURES as it can't be overridden if it came from the command line
_FEATURES := $(subst $(fcomma),$(fspace),$(FEATURES))

# $(1) feature, $(2) system progs, $(3) user progs, $(4) deps
define feature
FEATURES_DEFINED += $(1)
ifneq ($(filter $(1),$(_FEATURES)),)
$$(foreach n,$(4),$$(if $$(filter $$(n),$$(_FEATURES)),,$$(error feature $(1) needs $$(n))))
PROGS_SYSTEM += $(2)
PROGS_USER += $(3)
_TARWAKFEATURES += -f$(1)
endif
endef

$(eval $(call feature,net,sntp dhcpc,ip ping resolv tftp,))
$(eval $(call feature,telnetd,telnetd,,net))
$(eval $(call feature,initramfs,,,))
$(eval $(call feature,modules,modules,,))
$(eval $(call feature,mtd,,mtd,))
$(eval $(call feature,i2c,,i2c,))
$(eval $(call feature,gpio,,gpio,))
$(eval $(call feature,keeper,keeper,,))

# Make sure all of the requested features actually exist
$(foreach f,$(_FEATURES),$(if $(filter $(f),$(FEATURES_DEFINED)),,$(error unknown feature $(f))))

# Catch mistakes in either the feature evaluation or the all list
$(if $(filter-out $(FEATURES_ALL),$(FEATURES_DEFINED)),$(error missing from FEATURES_ALL: $(filter-out $(FEATURES_ALL),$(FEATURES_DEFINED))))
$(if $(filter-out $(FEATURES_DEFINED),$(FEATURES_ALL)),$(error in FEATURES_ALL but not defined: $(filter-out $(FEATURES_DEFINED),$(FEATURES_ALL))))
