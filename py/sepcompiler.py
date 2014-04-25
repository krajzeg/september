#!/usr/bin/env python3

##########################################################################
#
# sepcompiler
#
# The main compiler package - contains the back-end code to go from AST to
# bytecode. This is also the main entry point for the time being.
#
##########################################################################

from sepconstants import *
import sepparser as parser
import sepencoder as encoder

import collections

##############################################
# Exceptions
##############################################

class CompilationError(Exception):
    def __init__(self, node, message):
        self.node = node
        self.message = message
        super().__init__(message)

##############################################
# Function arguments
##############################################

class NamedArg:
    def __init__(self, name, value):
        self.name = name
        self.value = value


def create_function_arguments(function, node):
    args = []
    for arg_node in node.child("args").children:
        if arg_node.kind == parser.NamedArg:
            arg_name = arg_node.value
            arg_node = arg_node.child("expression")
        else:
            arg_name = None

        if arg_node.kind == parser.Constant:
            # add the constant as argument directly
            arg_value = arg_node.value
        elif arg_node.kind == parser.Block:
            # create a new function that pushes that block on the stack
            arg_value = function.compiler.create_expression_function(arg_node)
        else:
            # create a new function representing the expression used as
            # argument
            arg_value = function.compiler.create_expression_function(arg_node)

        if arg_name:
            args.append(NamedArg(arg_name, arg_value))
        else:
            args.append(arg_value)

    return args

##############################################
# Emitters
##############################################

# Emitter functions write code corresponding to AST nodes to a provided
# function object.

def emit_id(function, node):
    function.add(NOP, "lf", [node.value], [], [])


def emit_constant(function, node):
    function.add(PUSH, "", [], [node.value], [])


def emit_call(function, node):
    # fetch the function itself
    function.compile_node(node.first)
    arguments = create_function_arguments(function, node)
    function.add(LAZY, "", [], arguments, [])


def emit_subcall(function, node):
    # call the submethod
    arguments = create_function_arguments(function, node)
    function.add(LAZY, "f", [node.value], arguments, [])


def emit_complex_call(function, node):
    # compile all the subcalls in the chain
    for subcall in node.children:
        function.compile_node(subcall)
        # artificially inject the subcall that executes the chain
    function.compile_node(parser.Subcall("..!"))


def emit_binary_op(function, node):
    # several operators (mostly assignment) have to be special-cased

    if node.value == "=":
        # plain assignment is done by an operation flag "store"
        function.compile_node(node.first)
        function.compile_node(node.second)
        function.add(NOP, "s", [], [], [])
        return

    if node.value == ":=":
        # new variable creation is done by an operation flag "create property"

        # check if the left-hand side is an id
        if node.first.kind != parser.Id:
            # TODO: fix this error to contain the right token (requires
            # adding tokens to nodes).
            raise parser.ParsingException(
                ":= requires a single identifier on the left side.", None)

        # create the field first
        function.add(NOP, "lc", [node.first.value], [], [])
        # then assign it with the value provided
        function.compile_node(node.second)
        function.add(NOP, "s", [], [], [])
        return

    # non-special case - the operator is just a method
    function.compile_node(node.first)
    if node.second.kind == parser.Constant:
        function.add(LAZY, "f", [node.value], [node.second.value], [])
    else:
        subfunc = function.compiler.create_expression_function(node.second)
        function.add(LAZY, "f", [node.value], [subfunc], [])


def emit_unary_op(function, node):
    # unary operators are just methods with a funky name
    function.compile_node(node.first)
    function.add(LAZY, "f", [node.value], [], [])


def emit_block(function, node):
    # compile the block body as a new function
    params, body = node.child("parameters"), node.child("body")
    func = function.compiler.create_explicit_function(params, body)
    # at the point it was encountered, we have to push it on the stack
    function.add(PUSH, "", [], [func], [])

##############################################
# Matching emit_* functions to node types
##############################################

EMITTERS = {
    parser.Id: emit_id,
    parser.Constant: emit_constant,
    parser.FunctionCall: emit_call,
    parser.UnaryOp: emit_unary_op,
    parser.BinaryOp: emit_binary_op,
    parser.Block: emit_block,

    parser.ComplexCall: emit_complex_call,
    parser.Subcall: emit_subcall
}

##############################################
# Optimizers
##############################################

PRE_FLAGS = {F_PUSH_LOCALS, F_FETCH_PROPERTY, F_CREATE_PROPERTY}
POST_FLAGS = {F_POP_RESULT, F_STORE_VALUE}


def merge_nops_forward(func):
    """If the flags allow it, merges NOP operation with the next operation
    below them. For example NOP.lf and PUSH.s can be merged into PUSH.lfs.
    """
    code = func.code
    index = 0
    while index < len(code):
        opcode, flags, pre, args, post = code[index]
        flag_set = set(flags)

        only_pre_flags = flag_set.issubset(PRE_FLAGS)
        if opcode == NOP and only_pre_flags and index < len(code) - 1:
            n_opcode, n_flags, n_pre, n_args, n_post = code[index + 1]
            if flag_set.isdisjoint(set(n_flags)):
                # merge the nop.xxx as preamble to the next instruction
                code[index + 1] = (
                    n_opcode, flags + n_flags, pre + n_pre, args + n_args,
                    post + n_post)
                code = code[:index] + code[index + 1:]
            else:
                index += 1
        else:
            index += 1

    func.code = code


def merge_nops_backward(func):
    """If the flags allow it, merges NOP operation with the operation
    before them. For example LAZY.lf and NOP.s can be merged into LAZY.lfs.
    """
    code = func.code
    index = 0
    while index < len(code):
        opcode, flags, pre, args, post = code[index]
        flag_set = set(flags)

        only_post_flags = flag_set.issubset(POST_FLAGS)
        if opcode == NOP and only_post_flags and index > 0:
            p_opcode, p_flags, p_pre, p_args, p_post = code[index - 1]
            if flag_set.isdisjoint(set(p_flags)):
                # merge the nop.xxx into previous instruction
                code[index - 1] = (
                    p_opcode, p_flags + flags, p_pre + pre, p_args + args,
                    p_post + post)
                code = code[:index] + code[index + 1:]
            else:
                index += 1
        else:
            index += 1

    func.code = code


OPTIMIZERS = [merge_nops_forward, merge_nops_backward]

##############################################
# Optimizers
##############################################

class Compiler:
    """The compiler class for converting an abstract syntax tree into
    September bytecode at the specified output.
    """

    def __init__(self, output):
        self.output = output

    def compile(self, ast):
        """Compiles the provided AST to the output given when the compiler
        was created.
        """
        self.output.file_header()
        constant_mapping = ConstantCompiler(self.output).compile(ast)
        CodeCompiler(self.output, constant_mapping).compile(ast)
        self.output.file_footer()

##############################################
# Constant pool handling
##############################################

class ConstantCompiler:
    """This class goes over the entire AST and collects constant occurences.
    Once collected, those constants are written to the module's constant pool.
    """

    # The node types whose values are treated as needed constants.
    CONST_NODES = [
        parser.Id, parser.Constant,
        parser.BinaryOp, parser.UnaryOp,
        parser.ComplexCall, parser.Subcall, parser.NamedArg
    ]

    def __init__(self, output):
        self.output = output

    @staticmethod
    def walk_ast_tree(tree, callback):
        """Walks the AST, calling the callback at each node."""
        callback(tree)
        for child in tree.children:
            ConstantCompiler.walk_ast_tree(child, callback)

    def compile(self, ast):
        """Collects all the constants and writes the constant pool to the
        output. Returns a dict mapping each constant to its index in the
        constant pool."""

        # collect
        self.constant_occurrences = collections.defaultdict(int)
        self.walk_ast_tree(ast, self.add_occurrence)

        # sort them according to how often they occur
        all_constants = list(self.constant_occurrences.keys())
        all_constants.sort(key=lambda c: self.constant_occurrences[c],
                           reverse=True)

        # generate code for them
        self.output.constants_header(len(all_constants))
        for constant in all_constants:
            self.output.constant(constant)
        self.output.constants_footer()

        # return a constant -> constant index mapping
        return {constant: index + 1 for index, constant in
                enumerate(all_constants)}

    def add_occurrence(self, node):
        if node.kind in ConstantCompiler.CONST_NODES:
            self.constant_occurrences[node.value] += 1

##############################################
# Compiling actual code
##############################################

class Reference:
    """A reference into one of the pools (constant pool or function
    pool in the module.
    """
    def __init__(self, type, index):
        self.type = type
        self.index = index

    @classmethod
    def constant(cls, index):
        return cls(REF_CONSTANT, index)

    @classmethod
    def function(cls, index):
        return cls(REF_FUNCTION, index)

    @classmethod
    def argument_name(cls, index):
        return cls(REF_ARGNAME, index)


class CompiledFunction:
    """Represents a single September function."""

    def __init__(self, compiler, index, params=None):
        if params is None:
            params = []

        self.compiler = compiler
        self.index = index
        self.params = params
        self.code = []

    def compile_node(self, node):
        self.compiler.compile_node(self, node)

    def args_to_refs(self, args):
        """Converts operation arguments into references into the constant
        or function pool.
        """
        refs = []
        for arg in args:
            refs += self.compiler.create_references(arg)
        return refs

    def add(self, opcode, flags, pre_args, args, post_args):
        """Adds a new operation to the function."""
        pre_args, args, post_args = map(self.args_to_refs,
                                        [pre_args, args, post_args])
        self.code += [(opcode, flags, pre_args, args, post_args)]

class CodeCompiler:
    """Walks over the AST and compiles all the functions contained within,
    starting from the root function of the module.
    """

    def __init__(self, output, constants):
        self.output = output
        self.constants = constants
        self.functions = []

    def find_constant_index(self, constant):
        try:
            return self.constants[constant]
        except KeyError:
            raise CompilationError("Constant %s is not in the constant index." % constant)

    def create_main_function(self, body):
        """Creates the main function of a module using the provided Body node."""
        main_func = CompiledFunction(self, len(self.functions) + 1, [])
        return self.compile_function(main_func, body.children)

    def create_explicit_function(self, parameters, body):
        """Creates a new explicitly declared function with the given Parameters and Body."""
        func = CompiledFunction(self, len(self.functions) + 1, parameters.children)
        return self.compile_function(func, body.children)

    def create_expression_function(self, expression):
        """Creates a new function representing a provided expression node."""
        expr_func = CompiledFunction(self, len(self.functions) + 1, [])
        return self.compile_function(expr_func, [expression])

    def create_references(self, thing):
        """Takes a function or a constant, and creates an indexed reference into
        the constant or function pool."""
        if isinstance(thing, NamedArg):
            return ([Reference.argument_name(self.find_constant_index(thing.name))] +
                self.create_references(thing.value))
        if isinstance(thing, CompiledFunction):
            return [Reference.function(thing.index)]
        else:
            return [Reference.constant(self.find_constant_index(thing))]

    def compile_function(self, func, statements):
        """Compiles a previously created function, using the provided statements as its body."""
        self.functions.append(func)

        # compile default value expressions for the parameters
        for parameter in func.params:
            if parameter.child("default"):
                default_expr = parameter.child("default")
                if default_expr.kind == parser.Constant:
                    value = default_expr.value
                else:
                    value = self.create_expression_function(default_expr)
                parameter.default_value_ref = self.create_references(value)[0]

        # compile the body
        for statement in statements:
            self.compile_node(func, statement)
            func.add(NOP, F_POP_RESULT, [], [], [])

        return func

    def compile_node(self, target_function, node):
        """Emits code for a single node into the provided function."""
        emitter = EMITTERS[node.kind]
        if emitter is None:
            raise CompilationError(node,
                                   "Uncompilable node type: %s" % node.kind)
        emitter(target_function, node)

    def compile(self, ast):
        """Compiles the code in the provided AST into the compiler's output."""
        self.create_main_function(ast)

        for func in self.functions:
            for optimizer in OPTIMIZERS:
                optimizer(func)

        for func in self.functions:
            self.output.function_header(func.params)
            for line in func.code:
                self.output.code(*line)
            self.output.function_footer()

##############################################
# Debugging
##############################################

class StringOutput:
    """A special mock output class that prints the emitted code on the screen
     instead of writing it to a file."""

    def __init__(self):
        self.string = ""

    def ref_string(self, ref):
        return "%s%d" % (ref.type[0], ref.index)

    def constants_header(self, count):
        self.string += "CONSTANTS(%d):\n" % count

    def constant(self, value):
        self.string += "\tdefine\t%s\n" % repr(value)

    def constants_footer(self):
        self.string += "\n===\n\n"

    def function_header(self, params):
        param_names = []
        for param in params:
            param_string = param.value
            if P_NAMED_SINK in param.flags:
                param_string = ":::" + param_string
            if P_SINK in param.flags:
                param_string = "..." + param_string
            if P_LAZY_EVALUATED in param.flags:
                param_string = "?" + param_string
            if P_OPTIONAL in param.flags:
                param_string += "=<%s>" % self.ref_string(param.default_value_ref)
            param_names.append(param_string)

        self.string += "FUNC("
        self.string += ",".join(param_names)
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
        self.string += ", ".join([self.ref_string(arg) for arg in pre + args + post])
        self.string += "\n"

##############################################
# Main execution
##############################################

def debug_compile(ast):
    """Compiles the AST and outputs the resulting bytecode to the console."""
    output = StringOutput()
    Compiler(output).compile(ast)
    return output.string


def file_compile(ast, file):
    """Compiles the AST and outputs it to the file provided."""
    output = encoder.ModuleFileOutput(file)
    Compiler(output).compile(ast)


def print_error(error, location):
    lines = code.split("\n")

    if location is not None:
        line, column = location
    else:
        line = len(lines)
        column = len(lines[line - 1]) + 1

    print("Error at [%d:%d]: %s\n" % (line, column, error))

    offending_line = code.split("\n")[line - 1].replace("\t", " ")
    print(offending_line)
    print(" " * (column - 1) + "^")

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
            if extension != "sept":
                outname = basename + ".sept"
            else:
                outname = filename + ".sept"

        # try reading the file (UTF-8 assumed)
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
            # debugging enabled, compile it to the console first
            print(debug_compile(ast))
        with open(outname, "wb") as out:
            file_compile(ast, out)
    else:
        print("Usage:\n\tsepcompiler.py [-d] <source file> [<target file>]")
