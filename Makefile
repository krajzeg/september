# ==========================
# Global definitions
# ==========================

BIN_DIR := bin
LIB_DIR := lib
MODULE_DIR := src

PARTS := libseptvm interpreter

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
# Flags
# ==========================

CFLAGS += -Isrc/libseptvm

# ==========================
# Targets
# ==========================

.PHONY: tests all clean distclean

tests: all
	@echo ===============================
	@$(PYTHON) py/septests.py tests

all: $(foreach part,$(PARTS),$(part))

clean: $(foreach part,$(PARTS),$(part)-clean)
	$(RMDIR) $(LIB_DIR) $(BIN_DIR)
	
distclean: $(foreach part,$(PARTS),$(part)-distclean)
	$(RMDIR) $(LIB_DIR) $(BIN_DIR)

# ==========================
# Build dirs
# ==========================

$(LIB_DIR):
	$(MKDIR) $(LIB_DIR)

$(BIN_DIR):
	$(MKDIR) $(BIN_DIR)

# ==========================
# Global rules
# ==========================

# how to make a .d file
%.d: %.c
	$(CC) -MM $< -MT $(<:.c=.o) >$@

# ==========================
# Include part makefiles
# ==========================

MODULE_FILES = $(foreach part,$(PARTS),$(MODULE_DIR)/$(part)/part.mk)
include $(MODULE_FILES)
