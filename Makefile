.PHONY: all clean veryclean

all: interpreter tests

interpreter:
	@echo === Building interpreter...
	@$(MAKE) -C c/ all

tests: interpreter
	@echo === Running tests...
	@python py/septests.py tests

clean:
	@echo === Cleaning interpreter directory...
	@$(MAKE) -C c/ clean

veryclean:
	@echo === Cleaning interpreter directory...
	@$(MAKE) -C c/ veryclean
