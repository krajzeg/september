PLATFORM := Unix

# ==========================
# Toolchain
# ==========================

CFLAGS := -g -Wall -Wfatal-errors -DSEP_LOGGING_ENABLED

# ==========================
# Working with paths
# ==========================

# no change to paths needed under Unix
fix_paths = $1
