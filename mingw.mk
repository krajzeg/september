PLATFORM := MinGW

# ==========================
# Toolchain
# ==========================

CC := gcc
RM := del /q
CP := copy
MKDIR := mkdir
RMDIR := rmdir /q
CFLAGS := 
PYTHON := python
SEPTCOMPILER := $(PYTHON) py/sepcompiler.py

# ==========================
# Working with paths
# ==========================

# Under Windows, we have to use backslashes in paths passed to $(CP)/$(RM)
fix_paths = $(subst /,\,$1)
