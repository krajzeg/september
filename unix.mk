# ==========================
# Toolchain
# ==========================

CC = gcc
RM = rm -f
CFLAGS = -g -Wall -Wfatal-errors -DSEP_LOGGING_ENABLED
LDFLAGS = 
PYTHON = python3

# ==========================
# Adapt file paths (no change on Unix)
# ==========================

OBJECT_FILES = $(OBJECTS)
DEP_FILES = $(DEPS)

# ==========================
# Executable
# ==========================

OUT = 09

