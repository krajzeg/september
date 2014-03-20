# ==========================
# Global definitions
# ==========================

DIST_DIR := target
MODULE_DIR := src

MODULES := interpreter

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
# Targets
# ==========================

.PHONY: tests all clean distclean

tests: all
	@echo ===============================
	@$(PYTHON) py/septests.py tests

all: $(foreach module,$(MODULES),$(module))

clean: $(foreach module,$(MODULES),$(module)-clean)

distclean: $(foreach module,$(MODULES),$(module)-distclean)

# ==========================
# Global rules
# ==========================

# how to make a .d file
%.d: %.c
	$(CC) -MM $< -MT $(<:.c=.o) >$@

# ==========================
# Include module makefiles
# ==========================

MODULE_FILES = $(foreach module,$(MODULES),$(MODULE_DIR)/$(module)/module.mk)
include $(MODULE_FILES)
