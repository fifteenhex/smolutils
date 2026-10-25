# "Sets" - Like BSD sets or something
#
# Pass SETS="base core .." to select what you want, look at SETS_ALL for what
# you can have.

fempty :=
fspace := $(empty) $(empty)
fcomma := ,

# The sets below fill this in
PROGS =

SETS_ALL = base net initramfs core hwhandling filesystem misc
SETS ?= $(SETS_ALL)

# Internalise SETS as it can't be overridden if it came from the command line
_SETS := $(subst $(fcomma),$(fspace),$(SETS))

TAGS_ALL = modules i2c gpio mtd telnetd

# $(1) set, $(2) progs, $(3) deps
define set
SETS_DEFINED += $(1)
SET_DEPS_$(1) := $(3)
ifneq ($(filter $(1),$(_SETS)),)
$$(foreach n,$(3),$$(if $$(filter $$(n),$$(_SETS)),,$$(error set $(1) needs $$(n))))
PROGS += $(2)
# tarwak calls these features
_TARWAKFEATURES += -f$(1)
endif
endef

# $(1) set, $(2) tag, $(3) progs
define tagged
$(if $(filter $(1),$(SETS_DEFINED)),,$(error tag $(2) is in set $(1) which isn't defined yet))
$(if $(filter $(2),$(TAGS_ALL)),,$(error unknown tag $(2) in set $(1)))
ifneq ($(filter $(1),$(_SETS)),)
PROGS += $(3)
endif
endef

$(eval $(call set,base,init startup,))
$(eval $(call set,net,sntp dhcpc ip ping resolv tftp,))
$(eval $(call tagged,net,telnetd,telnetd))
$(eval $(call set,initramfs,,))
$(eval $(call set,core,getty keeper dmesg uname process su,))
$(eval $(call tagged,core,modules,modules))
$(eval $(call set,hwhandling,blkwak lsbus rtc devmem,))
$(eval $(call tagged,hwhandling,i2c,i2c))
$(eval $(call tagged,hwhandling,gpio,gpio))
$(eval $(call tagged,hwhandling,mtd,mtd))
$(eval $(call set,filesystem,filesystem files ls df,))
$(eval $(call set,misc,smolsh sha256sum xxd man less,))

# Make sure all of the requested sets actually exist
$(foreach s,$(_SETS),$(if $(filter $(s),$(SETS_DEFINED)),,$(error unknown set $(s))))

# Catch mistakes in either the set evaluation or the all list
$(if $(filter-out $(SETS_ALL),$(SETS_DEFINED)),$(error missing from SETS_ALL: $(filter-out $(SETS_ALL),$(SETS_DEFINED))))
$(if $(filter-out $(SETS_DEFINED),$(SETS_ALL)),$(error in SETS_ALL but not defined: $(filter-out $(SETS_DEFINED),$(SETS_ALL))))

# List out the sets for tools
.PHONY: list-sets
list-sets:
	@$(foreach s,$(SETS_ALL),echo $(s) $(SET_DEPS_$(s));)
