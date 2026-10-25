# "Sets" - Like BSD sets or something
#
# Pass SETS="net mtd .." to select what you want, look at SETS_ALL for what you
# can have. The base set is always there and lives in common.mk.

fempty :=
fspace := $(empty) $(empty)
fcomma := ,

SETS_ALL = net telnetd initramfs modules mtd i2c gpio keeper hwhandling
SETS ?= $(SETS_ALL)

# Internalise SETS as it can't be overridden if it came from the command line
_SETS := $(subst $(fcomma),$(fspace),$(SETS))

# $(1) set, $(2) system progs, $(3) user progs, $(4) deps
define set
SETS_DEFINED += $(1)
SET_DEPS_$(1) := $(4)
ifneq ($(filter $(1),$(_SETS)),)
$$(foreach n,$(4),$$(if $$(filter $$(n),$$(_SETS)),,$$(error set $(1) needs $$(n))))
PROGS_SYSTEM += $(2)
PROGS_USER += $(3)
# tarwak calls these features
_TARWAKFEATURES += -f$(1)
endif
endef

$(eval $(call set,net,sntp dhcpc,ip ping resolv tftp,))
$(eval $(call set,telnetd,telnetd,,net))
$(eval $(call set,initramfs,,,))
$(eval $(call set,modules,modules,,))
$(eval $(call set,mtd,,mtd,))
$(eval $(call set,i2c,,i2c,))
$(eval $(call set,gpio,,gpio,))
$(eval $(call set,keeper,keeper,,))
$(eval $(call set,hwhandling,,lsbus rtc devmem,))

# Make sure all of the requested sets actually exist
$(foreach s,$(_SETS),$(if $(filter $(s),$(SETS_DEFINED)),,$(error unknown set $(s))))

# Catch mistakes in either the set evaluation or the all list
$(if $(filter-out $(SETS_ALL),$(SETS_DEFINED)),$(error missing from SETS_ALL: $(filter-out $(SETS_ALL),$(SETS_DEFINED))))
$(if $(filter-out $(SETS_DEFINED),$(SETS_ALL)),$(error in SETS_ALL but not defined: $(filter-out $(SETS_DEFINED),$(SETS_ALL))))

# List out the sets for tools
.PHONY: list-sets
list-sets:
	@$(foreach s,$(SETS_ALL),echo $(s) $(SET_DEPS_$(s));)
