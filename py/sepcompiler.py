#!/usr/bin/python3

from sepconstants import *
import sepparser as parser
import sepencoder as encoder

########################################################################

class CompilationError(Exception):
    def __init__(self, node, message):
        self.node = node
        self.message = message
        super().__init__(message)

########################################################################

class ArgCount:
    def __init__(self, count):
        self.count = count

########################################################################

def emit_id(function, node):
    function.add(NOP, "lf", [node.value], [], [])
def emit_constant(function, node):
    function.add(PUSH, "", [], [node.value], [])

def extract_arguments(function, node):
    lazy_arguments = []
    for arg_node in node.child("args").children:
        if arg_node.kind == parser.Constant:
            lazy_arguments.append(arg_node.value)
        elif arg_node.kind == parser.Block:
            block_func = function.compiler.create_function(arg_node)
            lazy_arguments.append(block_func)
        else:
            lazy_arguments.append(function.compiler.create_function(arg_node))
    return lazy_arguments

def emit_call(function, node):
    # fetch the function itself
    function.compile_node(node.first)
    arguments = extract_arguments(function, node)
    function.add(LAZY, "", [], arguments, [])

def emit_subcall(function, node):
    # call the submethod
    arguments = extract_arguments(function, node)
    function.add(LAZY, "f", [node.value], arguments, [])

def emit_complex_call(function, node):
    # compile all the subcalls in the chain
    for subcall in node.children:
        function.compile_node(subcall)

    # inject the execution subcall
    function.compile_node(parser.Subcall("..!"))

def emit_binary_op(function, node):
    # several operators (mostly assignment) are special-cased

    if node.value == "=":
        # plain assignment
        function.compile_node(node.first)
        function.compile_node(node.second)
        function.add(NOP, "s", [], [], [])
        return

    if node.value == ":=":
        # new variable assignment
        if node.first.kind != parser.Id:
            raise parser.ParsingException(":= requires a single identifier on the left side.", None)

        # compile it
        # create the field first
        function.add(NOP, "lc", [node.first.value], [], [])
        # then assign it with the value provided
        function.compile_node(node.second)
        function.add(NOP, "s", [], [], [])
        return

    # lazy binary ops
    function.compile_node(node.first)
    if node.second.kind == parser.Constant:
        function.add(LAZY, "f", [node.value], [node.second.value], [])
    else:
        subfunc = function.compiler.create_function(node.second)
        function.add(LAZY, "f", [node.value], [subfunc], [])

def emit_unary_op(function, node):
    function.compile_node(node.first)
    function.add(LAZY, "f", [node.value], [], []) # TODO: should be EAGER, once that's done

def emit_block(function, node):
    # compile the block body separately
    block = function.compiler.create_function(node.child("body"))
    # just push it here
    function.add(PUSH, "", [], [block], [])

EMITTERS = {
    parser.Id:           emit_id,
    parser.Constant:     emit_constant,
    parser.FunctionCall: emit_call,
    parser.UnaryOp:      emit_unary_op,
    parser.BinaryOp:     emit_binary_op,
    parser.Block:        emit_block,

    parser.ComplexCall:  emit_complex_call,
    parser.Subcall:      emit_subcall
}

########################################################################

PRE_FLAGS  = {F_PUSH_LOCALS, F_FETCH_PROPERTY, F_CREATE_PROPERTY}
POST_FLAGS = {F_POP_RESULT, F_STORE_VALUE}

def merge_nops_forward(func):
    code = func.code
    index = 0
    while index < len(code):
        opcode, flags, pre, args, post = code[index]
        flag_set = set(flags)

        only_pre_flags = flag_set.issubset(PRE_FLAGS)
        if opcode == NOP and only_pre_flags and index < len(code) - 1:
            n_opcode, n_flags, n_pre, n_args, n_post = code[index+1]
            if flag_set.isdisjoint(set(n_flags)):
                # merge the nop.xxx as preamble to the next instruction
                code[index+1] = (n_opcode, flags + n_flags, pre + n_pre, args + n_args, post + n_post)
                code = code[:index] + code[index+1:]
            else:
                index += 1
        else:
            index += 1

    func.code = code

def merge_nops_backward(func):
    code = func.code
    index = 0
    while index < len(code):
        opcode, flags, pre, args, post = code[index]
        flag_set = set(flags)

        only_post_flags = flag_set.issubset(POST_FLAGS)
        if opcode == NOP and only_post_flags and index > 0:
            p_opcode, p_flags, p_pre, p_args, p_post = code[index-1]
            if flag_set.isdisjoint(set(p_flags)):
                # merge the nop.xxx into previous instruction
                code[index-1] = (p_opcode, p_flags + flags, p_pre + pre, p_args + args, p_post + post)
                code = code[:index] + code[index+1:]
            else:
                index += 1
        else:
            index += 1

    func.code = code

OPTIMIZERS = [merge_nops_forward, merge_nops_backward]

########################################################################

class Compiler:
    def __init__(self, output):
        self.output = output

    def compile(self, ast):
        self.output.file_header()
        constant_mapping = ConstantCompiler(self.output).compile(ast)
        CodeCompiler(self.output, constant_mapping).compile(ast)
        self.output.file_footer()

########################################################################

class ConstantCompiler:
    CONST_NODES = [
        parser.Id, parser.Constant,
        parser.BinaryOp, parser.UnaryOp,
        parser.ComplexCall, parser.Subcall
    ]

    def __init__(self, output):
        self.output = output

    @staticmethod
    def walk_ast_tree(tree, callback):
        callback(tree)
        for child in tree.children:
            ConstantCompiler.walk_ast_tree(child, callback)

    def compile(self, ast):
        # collect all needed constants together with how much they occur
        self.constant_occurrences = {}
        self.walk_ast_tree(ast, self.add_occurrence)

        # sort them according to how much they occur
        all_constants = list(self.constant_occurrences.keys())
        all_constants.sort(key=lambda c: self.constant_occurrences[c], reverse=True)

        # generate code for them
        self.output.constants_header(len(all_constants))
        for constant in all_constants:
            self.output.constant(constant)
        self.output.constants_footer()

        # return constant -> constant index mapping
        return {constant: index+1 for index, constant in enumerate(all_constants)}

    def add_occurrence(self, node):
        if node.kind in ConstantCompiler.CONST_NODES:
            self.constant_occurrences[node.value] = self.constant_occurrences.get(node.value, 0) + 1

########################################################################

class CompiledFunction:
    def __init__(self, compiler, index, args=None):
        if args is None:
            args = []

        self.compiler = compiler
        self.index = index
        self.args = args
        self.code = []

    def compile_node(self, node):
        self.compiler.compile_node(self, node)

    def args_to_ints(self, args):
        actual_args = []
        for arg in args:
            if isinstance(arg, CompiledFunction):
                actual_args += [-arg.index]
            elif isinstance(arg, ArgCount):
                actual_args += [arg.count]
            else:
                try:
                    constant_index = self.compiler.constants[arg]
                    actual_args += [constant_index]
                except KeyError:
                    raise CompilationError(None, "Constant '%s' is not present in index." % arg)
        return actual_args
    
    def add(self, opcode, flags, pre_args, args, post_args):
        pre_args, args, post_args = map(self.args_to_ints, [pre_args, args, post_args])
        self.code += [(opcode, flags, pre_args, args, post_args)]


class CodeCompiler:
    def __init__(self, output, constants):
        self.output = output
        self.constants = constants
        self.functions = []

    def create_function(self, body):
        if body.kind != parser.Body:
            statements = [body]
        else:
            statements = body.children

        func = CompiledFunction(self, len(self.functions)+1, [])
        self.functions.append(func)

        for statement in statements:
            self.compile_node(func, statement)
            func.add(NOP, F_POP_RESULT, [], [], [])

        return func

    def compile_node(self, target_function, node):
        emitter = EMITTERS[node.kind]
        if emitter is None:
            raise CompilationError(node, "Uncompilable node type: %s" % node.kind)
        emitter(target_function, node)

    def compile(self, ast):
        self.create_function(ast)

        for func in self.functions:
            for optimizer in OPTIMIZERS:
                optimizer(func)

        for func in self.functions:
            self.output.function_header([])
            for line in func.code:
                self.output.code(*line)
            self.output.function_footer()

class StringOutput:
    def __init__(self):
        self.string = ""

    def constants_header(self, count):
        self.string += "CONSTANTS(%d):\n" % count

    def constant(self, value):
        self.string += "\tdefine\t%s\n" % repr(value)

    def constants_footer(self):
        self.string += "\n===\n\n"

    def function_header(self, args):
        self.string += "FUNC("
        self.string += ",".join(args)
        self.string += "):\n"

    def function_footer(self):
        self.string += "\n"

    def file_header(self):
        pass

    def file_footer(self):
        pass

    def code(self, opcode, flags, pre, args, post):
        operation = "\t%s%s" % (opcode, "." + flags if flags else "")
        self.string += operation + " " * (14 - len(operation))
        self.string += ", ".join([str(arg) for arg in pre + args + post])
        self.string += "\n"


def debug_compile(ast):
    output = StringOutput()
    Compiler(output).compile(ast)
    return output.string

def file_compile(ast, file):
    output = encoder.ModuleFileOutput(file)
    Compiler(output).compile(ast)

def print_error(error, location):
    lines = code.split("\n")

    if location is not None:
        line, column = location
    else:
        line = len(lines)
        column = len(lines[line-1]) + 1

    print("Error at [%d:%d]: %s\n" % (line, column, error))

    offending_line = code.split("\n")[line-1].replace("\t", " ")
    print(offending_line)
    print(" " * (column-1) + "^")

    sys.exit(1)

if __name__ == "__main__":
    import sys
    import os.path
    import seplexer as lexer

    if len(sys.argv) > 1:
        # bytecode debugging turned on?
        debug_output = False
        if sys.argv[1] == "-d":
            debug_output = True
            sys.argv[1:2] = []

        # extract input and output filename filename
        filename = sys.argv[1]
        if len(sys.argv) > 2:
            outname = sys.argv[2]
        else:
            basename, extension = os.path.splitext(filename)
            if extension != "09":
                outname = basename + ".09"
            else:
                outname = filename + ".09"

        with open(filename, "r", encoding="utf8") as f:
            try:
                code = f.read()
            except UnicodeDecodeError as ude:
                print("Error: Input file was not proper UTF-8. Offending byte "
                      "at position %d." % ude.start)
                sys.exit(1)

        # parse the code
        ast = None
        try:
            ast = parser.parse(lexer.lex(code))
        except parser.ParsingException as pe:
            print_error(pe, pe.token.location if pe.token else None)
        except lexer.LexerException as le:
            print_error(le, le.location)

        # compile the code
        if debug_output:
            print(debug_compile(ast))
        with open(outname, "wb") as out:
            file_compile(ast, out)
    else:
        print("Usage:\n\tsepcompiler.py [-d] <source file> [<target file>]")