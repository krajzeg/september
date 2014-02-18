from crconstants import *

import crparser as parser
import crencoder

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

def emit_call(function, node):
    # fetch the function itself
    function.compile_node(node.first)
    # create arguments
    lazy_arguments = []
    for arg_node in node.second.children:
        if arg_node.kind == parser.Constant.KIND:
            lazy_arguments.append(arg_node.value)
        elif arg_node.kind == parser.Block.KIND:
            block_func = function.compiler.create_function(arg_node)
            lazy_arguments.append(block_func)
        else:
            lazy_arguments.append(function.compiler.create_function(arg_node))
    function.add(LAZY, "", [], lazy_arguments, [])

def emit_binary_op(function, node):
    # assignment is special-cased
    if node.value == "=":
        function.compile_node(node.first)
        function.compile_node(node.second)
        function.add(NOP, "s", [], [], [])
        return

    # lazy binary ops
    function.compile_node(node.first)
    if node.second.kind == parser.Constant.KIND:
        function.add(LAZY, "f", [node.value], [node.second.value], [])
    else:
        subfunc = function.compiler.create_function(node.second)
        function.add(LAZY, "f", [node.value], [subfunc], [])

def emit_unary_op(function, node):
    function.compile_node(node.first)
    function.add(LAZY, "f", [node.value], [], []) # TODO: should be EAGER, once that's done

def emit_block(function, node):
    # compile the block body separately
    block = function.compiler.create_function(node.body)
    # just push it here
    function.add(PUSH, "", [], [block], [])

EMITTERS = {
    parser.Id.KIND:          emit_id,
    parser.Constant.KIND:    emit_constant,
    parser.FCall.KIND:       emit_call,
    parser.UnaryOp.KIND:     emit_unary_op,
    parser.BinaryOp.KIND:    emit_binary_op,
    parser.Block.KIND:       emit_block
}

########################################################################

PRE_FLAGS  = set([F_PUSH_LOCALS,F_FETCH_PROPERTY])
POST_FLAGS = set([F_POP_RESULT, F_STORE_VALUE])

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
        if node.kind in [parser.Id.KIND, parser.Constant.KIND, parser.BinaryOp.KIND, parser.UnaryOp.KIND]:
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
        if body.kind != parser.Body.KIND:
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
        main = self.create_function(ast)

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
    output = crencoder.ModuleFileOutput(file)
    Compiler(output).compile(ast)


import sys
import lexer
import os.path

if len(sys.argv) > 1:
    filename = sys.argv[1]
    if len(sys.argv) > 2:
        outname = sys.argv[2]
    else:
        basename, extension = os.path.splitext(filename)
        if extension != "09":
            outname = basename + ".09"
        else:
            outname = filename + ".09"

    with open(filename, "r") as f:
        code = f.read()

    ast = parser.parse(lexer.lex(code))

    print(debug_compile(ast))
    with open(outname, "wb") as out:
        file_compile(ast, out)