# ==========================
# Choose platform
# ==========================

ifneq (,$(findstring Windows,$(OS)))
	ifneq (,$(MINGDIR))
		include mingw.mk
	else
$(error Under Windows, only MinGW is supported, and MINGDIR is not set.)
	endif
else
	include unix.mk
endif

# ==========================
# Configuration
# ==========================

include config.mk

ifeq ($(GC_STRESS_TEST),1)
	CFLAGS += -DSEP_GC_STRESS_TEST
endif
ifeq ($(PROPMAP_STRESS_TEST),1)
	CFLAGS += -DSEP_PROPMAP_STRESS_TEST
endif

ifeq ($(BUILD_TYPE),debug)
	CFLAGS += -g -Wall -Wfatal-errors -Werror -DSEP_DEBUG
else ifeq ($(BUILD_TYPE),release)
	CFLAGS = -O2 -ffast-math
else
$(error Build type should be 'debug' or 'release'.)
endif

# ==========================
# Global definitions
# ==========================

BIN_DIR := bin
MODULES_DIR := bin/modules
LIB_DIR := lib

PARTS_DIR := src
PARTS := libseptvm runtime interpreter

# ==========================
# Flags
# ==========================

INCLUDE_DIRS := -Isrc/libseptvm
CFLAGS += $(INCLUDE_DIRS)

# ==========================
# Targets
# ==========================

.PHONY: tests all clean distclean

tests: all
	@echo ===============================
	@$(PYTHON) py/septests.py tests

all: $(foreach part,$(PARTS),$(part))

clean: $(foreach part,$(PARTS),$(part)-clean)
	-$(RMDIR) $(call fix_paths,$(MODULES_DIR) $(LIB_DIR) $(BIN_DIR))
	
distclean: $(foreach part,$(PARTS),$(part)-distclean)
	-$(RMDIR) $(call fix_paths,$(MODULES_DIR) $(LIB_DIR) $(BIN_DIR))

# ==========================
# Build dirs
# ==========================

$(LIB_DIR):
	$(MKDIR) $(LIB_DIR)

$(BIN_DIR):
	$(MKDIR) $(BIN_DIR)
	
$(MODULES_DIR): | $(BIN_DIR)
	$(MKDIR) $(call fix_paths,$(MODULES_DIR))

# ==========================
# Global rules
# ==========================

# how to make a .d file
%.d: %.c
	$(CC) $(INCLUDE_DIRS) -MM $< -MT $(<:.c=.o) >$@

# ==========================
# Include part makefiles
# ==========================

PART_FILES = $(foreach part,$(PARTS),$(PARTS_DIR)/$(part)/part.mk)
include $(PART_FILES)
