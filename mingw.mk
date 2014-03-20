PLATFORM := MinGW

# ==========================
# Toolchain
# ==========================

CC := gcc
RM := del /q
CP := copy
CFLAGS := -g -Wall -Wfatal-errors -DSEP_LOGGING_ENABLED
PYTHON := python

# ==========================
# Working with paths
# ==========================

# Under Windows, we have to use backslashes in paths passed to $(CP)/$(RM)
fix_paths = $(subst /,\,$1)
