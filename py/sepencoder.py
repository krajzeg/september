##########################################################################
#
# sepencoder
#
# This package is responsible for writing the on-disk ".09" format that
# September uses for its bytecode.
#
##########################################################################

import math

from sepconstants import *

##############################################
# Constants
##############################################

### Opcodes representing instructions
OPCODE_ENCODING = {
    NOP: 0x0,
    PUSH: 0x1,
    LAZY: 0x4
}

### Bitmask for particular operation flags
FLAG_ENCODING = {
    F_PUSH_LOCALS:    0x80,
    F_FETCH_PROPERTY: 0x40,
    F_CREATE_PROPERTY: 0x20,
    F_STORE_VALUE:    0x10,
    F_POP_RESULT:     0x08,
}

### Constant types writable to the file
# Note: float is not yet supported
CONSTANT_TYPE = {
    int: 0x1,
    str: 0x2
}

##############################################
# Constants
##############################################

def encode_operation(operation, flags):
    """Encodes a flagged operation as a single-byte opcode."""
    encoding = OPCODE_ENCODING[operation]
    for flag in flags:
        encoding |= FLAG_ENCODING[flag]
    return encoding

def is_vararg(operation):
    """True if the given operation has a variable number of arguments."""
    return operation == LAZY

###################################################

class ModuleFileOutput:
    """A compiler output that targets an on-disk file."""
    def __init__(self, file):
        self.file = file

    def _write_byte(self, byte_value):
        """Writes a single byte to the file."""
        self.file.write(bytes([byte_value]))

    def _write_int(self, int_value):
        """Encodes an integer inside the file.
        The encoding is variable sized. Integers -63..63 are saved on a
        single byte, 0xS0MMMMMM, sign-magnitude format. Integers -8191..8191
        are saved on two bytes, 0xS10MMMMM MMMMMMMM. Larger integers are
        saved with the first byte storing the sign and the number of bytes
        required for the magnitude, and the magnitude is simply encoded (
        big-endian) on however many bytes it needs.
        """
        if int_value < 0:
            sign = 1
            int_value = -int_value
        else:
            sign = 0
        if int_value < 64:
            self._write_byte(sign * 0x80 | int_value)
        elif int_value < 8192:
            self._write_byte(sign * 0x80 | 0x40 | ((int_value & 0x1f00) >> 8))
            self._write_byte(int_value & 0xff)
        else:
            req_bytes = int(math.log(int_value, 256))
            self._write_byte(sign * 0x80 | 0x60 | req_bytes)
            shift = 8 * (req_bytes-1)
            mask = 0xff << shift
            while mask != 0:
                self._write_byte((int_value & mask) >> shift)
                mask >>= 8
                shift -= 8

    def write_str(self, str_value):
        """Encodes a string inside the file.
        The encoding is (int:string length)(utf8 encoded bytes).
        """
        utf8encoding = str_value.encode("utf_8")
        self._write_int(len(utf8encoding))
        self.file.write(utf8encoding)

    def constants_header(self, num_constants):
        """Writes the module constants header for the given number of
        constants."""
        self._write_int(num_constants)
        pass

    def constant(self, value):
        """Writes a constant, encoded as (constant type)(value)."""
        # encode the type
        try:
            self._write_byte(CONSTANT_TYPE[value.__class__])
        except KeyError:
            raise Exception("Don't know how to write down constant of type: %s.", value.__class__)

        if isinstance(value, int):
            self._write_int(value)
        if isinstance(value, str):
            self.write_str(value)

    def constants_footer(self):
        """Writes the module constants footer after all the constants are
        encoded (currently there is no footer needed)."""
        pass

    def function_header(self, args):
        """Writes the function header."""
        # TODO: function parameters are not supported yet
        self._write_int(0)

    def function_footer(self):
        """Writes the function footer after the function's code is encoded."""
        self._write_byte(0xff)

    def code(self, opcode, flags, pre, args, post):
        """Writes a single instruction with the parameters provided.
        Arguments consumed by flags should be in the 'pre' or 'post'
        parameter, depending on whether this flag executes before the
        operation or after.
        """
        self._write_byte(encode_operation(opcode, flags))
        for arg in pre:
            self._write_int(arg)
        if is_vararg(opcode):
            self._write_int(len(args))
        for arg in args:
            self._write_int(arg)
        for arg in post:
            self._write_int(arg)

    def file_header(self):
        """Writes the file header."""
        self.file.write("SEPT".encode("ascii"))

    def file_footer(self):
        """Writes the file footer."""
        self._write_byte(0xff)

