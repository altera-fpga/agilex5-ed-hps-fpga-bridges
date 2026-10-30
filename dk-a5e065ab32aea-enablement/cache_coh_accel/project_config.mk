# Define the Quartus project name
PROJECT_NAME := top

#Define the Quartus revision names
REVISION_NAMES := \
	cache_coh_accel \

# Define the base revision
BASE_REVISION := $(firstword $(REVISION_NAMES))

# Enable functional simulation
SIM_ENABLED := 1

# Disable strict validation of README.md (kit not yet in Quartus 26.3.1 kit list)
NO_ENFORCE_README_VALIDATION := 1