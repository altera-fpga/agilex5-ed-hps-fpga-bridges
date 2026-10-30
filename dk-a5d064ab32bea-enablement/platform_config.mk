# Create rules for datamover subdir
TARGET_SUBDIR := datamover
$(eval $(call create_targets_on_subdir, $(TARGET_SUBDIR)))
$(foreach target,  $(shell make  --no-print-directory -q -C $(TARGET_SUBDIR) print-sw-targets), $(eval $(call create_sw_targets_on_subdir, $(TARGET_SUBDIR), $(target))))
