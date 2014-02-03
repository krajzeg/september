CC = gcc
RM = del
CFLAGS = -g -Wall -Wfatal-errors -DSEP_LOGGING_ENABLED
LDFLAGS = 
OBJECTS = common/errors.o common/debugging.o common/garray.o \
  vm/module.o vm/mem.o vm/types.o vm/objects.o vm/arrays.o vm/functions.o vm/vm.o \
  vm/code.o vm/exceptions.o vm/stack.o \
  io/decoder.o \
  runtime/runtime.o runtime/stringp.o runtime/objectp.o \
  september.o

OBJECTS_WIN = $(subst /,\,$(OBJECTS))

OUT = 09.exe


$(OUT): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o$(OUT)

clean:
	$(RM) $(OUT) $(OBJECTS_WIN)

.PHONY: clean
