# ==========================
# File lists
# ==========================

INTP_DIR := src/interpreter

INTP_SOURCE_DIRS := src/interpreter/common \
  src/interpreter/io \
  src/interpreter/runtime \
  src/interpreter/vm \
  src/interpreter

INTP_SOURCE_FILES := $(foreach dir,$(INTP_SOURCE_DIRS),$(wildcard $(dir)/*.c))

INTP_OBJECTS = $(INTP_SOURCE_FILES:.c=.o)
INTP_DEPENDENCIES = $(INTP_SOURCE_FILES:.c=.d)

# ==========================
# System dependent parts
# ==========================

ifeq ($(PLATFORM),MinGW)
  INTP_EXEC_NAME = 09.exe
else
  INTP_EXEC_NAME = 09
endif
INTP_EXECUTABLE := $(DIST_DIR)/$(INTP_EXEC_NAME)

# ==========================
# Executable
# ==========================

.PHONY: interpreter interpreter-clean interpreter-distclean

interpreter: $(INTP_EXECUTABLE)

$(INTP_EXECUTABLE): $(INTP_OBJECTS)
	$(CC) $(INTP_OBJECTS) $(LDFLAGS) -o$(INTP_EXECUTABLE)

# ==========================
# Cleaning
# ==========================

interpreter-clean:
	$(RM) $(call fix_paths,$(INTP_EXECUTABLE) $(INTP_OBJECTS))

interpreter-distclean:
	$(RM) $(call fix_paths,$(INTP_EXECUTABLE) $(INTP_OBJECTS) $(INTP_DEPENDENCIES))

# ==========================
# Autogenerated dependencies
# ==========================

# cleaning does not need dependencies
ifeq (,$(findstring clean,$(MAKECMDGOALS)))
    -include $(INTP_DEPENDENCIES)
endif