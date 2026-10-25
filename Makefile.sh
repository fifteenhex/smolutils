CPU ?= sh4

ifeq ($(filter $(CPU),sh4),)
$(error CPU=$(CPU) is not sh4)
endif

SMOL_ARCH = $(CPU)

include common.mk

all: elfs

ELFS = $(addsuffix .$(CPU).elf,$(PROGS))

.PHONY: elfs
elfs: $(ELFS)

.PHONY: sh-$(CPU).tar
sh-$(CPU).tar: rootfs.tarwak.json elfs $(CONFIGTAGS) $(EXTRA_TARS)
	$(call build_tar,$@,%s.$(CPU).elf)

.PHONY: sh-$(CPU).erofs
sh-$(CPU).erofs: sh-$(CPU).tar
	$(MSG) EROFS $@
	$(Q)$(EROFS_CMD) $@ $<

.PHONY: sh-$(CPU).cpio.gz
sh-$(CPU).cpio.gz: sh-$(CPU).tar
	$(MSG) CPIO $@
	$(Q)bsdtar --format=newc -cf - @$< | gzip -9 > $@

.PHONY: clean
clean:
	$(Q)rm -f $(SMOL_BUILDMODE) $(LOCAL_CONFIG_STAMP) $(CONFIGTAGS)
	$(Q)rm -f *.$(CPU).o *.$(CPU).elf *.$(CPU).elf.dbg
	$(Q)rm -f sh-$(CPU).tar sh-$(CPU).erofs sh-$(CPU).cpio.gz
