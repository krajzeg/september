PLATFORM := Unix

# ==========================
# Toolchain
# ==========================

CFLAGS := -g -Wall -Wfatal-errors -Werror -DSEP_DEBUG -fPIC
PYTHON := python3
MKDIR := mkdir
RMDIR := rmdir
SEPTCOMPILER := py/sepcompiler.py

# ==========================
# Working with paths
# ==========================

# Under Windows, we have to use backslashes in paths passed to $(CP)/$(RM)
fix_paths = $1

