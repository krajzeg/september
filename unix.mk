PLATFORM := Unix

# ==========================
# Toolchain
# ==========================

CFLAGS := -g -Wall -Wfatal-errors -Werror -DSEP_LOGGING_ENABLED
PYTHON := python3
MKDIR := mkdir
RMDIR := rmdir

# ==========================
# Working with paths
# ==========================

# Under Windows, we have to use backslashes in paths passed to $(CP)/$(RM)
fix_paths = $1

