import math

from sepconstants import *

###################################################

OPCODE_ENCODING = {
    NOP: 0,
    PUSH: 1,
    LAZY: 4,
    EAGER: 5
}

FLAG_ENCODING = {
    F_PUSH_LOCALS:    0x80,
    F_FETCH_PROPERTY: 0x40,
    F_CREATE_PROPERTY: 0x20,
    F_STORE_VALUE:    0x10,
    F_POP_RESULT:     0x08,
}

CONSTANT_TYPE = {
    int: 0x1,
    str: 0x2
}

###################################################

def encode_operation(opcode, flags):
    encoding = OPCODE_ENCODING[opcode]
    for flag in flags:
        encoding |= FLAG_ENCODING[flag]
    return encoding

def is_vararg(opcode):
    return opcode == LAZY

###################################################

class ModuleFileOutput:
    def __init__(self, file):
        self.file = file

    def write_byte(self, byte_value):
        self.file.write(bytes([byte_value]))

    def write_int(self, int_value):
        if int_value < 0:
            sign = 1
            int_value = -int_value
        else:
            sign = 0
        if int_value < 64:
            self.write_byte(sign * 0x80 | int_value)
        elif int_value < 8192:
            self.write_byte(sign * 0x80 | 0x40 | ((int_value & 0x1f00) >> 8))
            self.write_byte(int_value & 0xff)
        else:
            req_bytes = math.log(int_value, 256)
            self.write_byte(sign * 0x80 | 0x60 | req_bytes)
            shift = 8 * (req_bytes-1)
            mask = 0xff << shift
            while mask != 0:
                self.write_byte((int_value & mask) >> shift)
                mask >>= 8
                shift -= 8

    def write_str(self, str_value):
        utf8encoding = str_value.encode("utf_8")
        self.write_int(len(utf8encoding))
        self.file.write(utf8encoding)

    def constants_header(self, num_constants):
        # no header needed
        self.write_int(num_constants)
        pass

    def constant(self, value):
        # encode the type
        try:
            self.write_byte(CONSTANT_TYPE[value.__class__])
        except KeyError:
            raise Exception("Don't know how to write down constant of type: %s.", value.__class__)

        if isinstance(value, int):
            self.write_int(value)
        if isinstance(value, str):
            self.write_str(value)

    def constants_footer(self):
        pass

    def function_header(self, args):
        # no args (args will be supported later)
        self.write_int(0)

    def function_footer(self):
        self.write_byte(0xff)

    def code(self, opcode, flags, pre, args, post):
        self.write_byte(encode_operation(opcode, flags))
        for arg in pre:
            self.write_int(arg)
        if is_vararg(opcode):
            self.write_int(len(args))
        for arg in args:
            self.write_int(arg)
        for arg in post:
            self.write_int(arg)

    def file_header(self):
        self.file.write("SEPT".encode("ascii"))

    def file_footer(self):
        self.write_byte(0xff)

