##########################################################################
#
# sepconstants
#
# Some common constants used to represent September bytecode inside the
# compiler.
#
##########################################################################

### Opcode constants
NOP = "nop"
PUSH = "push"
LAZY = "lazy"

### Operation flags
F_PUSH_LOCALS = "l"
F_FETCH_PROPERTY = "f"
F_CREATE_PROPERTY = "c"
F_STORE_VALUE = "s"
F_POP_RESULT = "v"

### Parameter flags
P_LAZY_EVALUATED = "lazy"
P_SINK = "sink"
