.PHONY: all clean veryclean

ifneq (,$(findstring Windows,$(OS)))
  PYTHON=python
else
  PYTHON=python3
endif

all: interpreter tests

interpreter:
	@echo === Building interpreter...
	@$(MAKE) -C c/ all

tests: interpreter
	@echo === Running tests...
	@$(PYTHON) py/septests.py tests

clean:
	@echo === Cleaning interpreter directory...
	@$(MAKE) -C c/ clean

veryclean:
	@echo === Cleaning interpreter directory...
	@$(MAKE) -C c/ veryclean

