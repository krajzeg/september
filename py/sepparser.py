##########################################################################
#
# sepparser
#
# This package implements a parser for September, using an approach based on
# Top-Down Operator Precedence (http://javascript.crockford.com/tdop/tdop
# .html), adapted to make it more readable and suitable to Python.
#
##########################################################################

import seplexer as lexer

##############################################
# Exceptions
##############################################

class ParsingException(Exception):
    def __init__(self, message, token):
        self.token = token
        super().__init__(message)

##############################################
# The AST node
##############################################

class Node:
    """Represents a single node in the abstract syntax tree."""
    def __init__(self, kind, value=None, children_names=None):
        self._kind = (kind,) # wrapping in a tuple prevents method-assigning
                             # magic that Python does
        self.value = value
        self.children = []
        if children_names:
            # create a mapping from name to its index
            self.name_map = dict(zip(children_names,
                                     range(len(children_names))))
        else:
            self.name_map = None

    @property
    def kind(self):
        return self._kind[0]

    def child(self, index_or_name):
        if isinstance(index_or_name, int):
            # treat as an index
            if len(self.children) > index_or_name:
                return self.children[index_or_name]
            else:
                raise Exception("Internal error: node %s does not have a "
                                "child with index %d" % (self.kind,
                                                         index_or_name))
        else:
            # treat as a name
            if self.name_map and index_or_name in self.name_map:
                return self.child(self.name_map[index_or_name])
            else:
                return None

    def set_child(self, index, value):
        if len(self.children) <= index:
            self.children += [None] * (index - len(self.children) + 1)
        self.children[index] = value

    @property
    def first(self):
        return self.child(0)

    @property
    def second(self):
        return self.child(1)

    @property
    def third(self):
        return self.child(2)

    @first.setter
    def first(self, value):
        self.set_child(0, value)

    @second.setter
    def second(self, value):
        self.set_child(1, value)

    @third.setter
    def third(self, value):
        self.set_child(2, value)

    def add(self, child):
        """Adds a new child node."""
        self.children.append(child)

    def __str__(self):
        if self.value is not None:
            return "%s(%s)" % (self.kind, self.value)
        else:
            return self.kind

    def __repr__(self):
        return self.indented_string(0)

    def indented_string(self, indent_level):
        text = " " * indent_level + str(self) + "\n"
        for child in self.children:
            text += child.indented_string(indent_level + 1)
        return text

##############################################
# Individual node types
##############################################

## leaf nodes
def Id(name):
    return Node(Id, name)

def Constant(value):
    return Node(Constant, value)

## operators
def UnaryOp(operator, operand):
    node = Node(UnaryOp, "unary" + operator, ["operand"])
    node.first = operand
    return node

def BinaryOp(operator, left, right):
    node = Node(BinaryOp, operator, ["left", "right"])
    node.children = [left, right]
    return node

## function calls and related nodes
def FunctionCall(call_target):
    node = Node(FunctionCall, children_names=["target", "args"])
    node.children = [call_target, Arguments()]
    return node

def Arguments():
    return Node(Arguments)

def ComplexCall(*calls):
    node = Node(ComplexCall, "..!", ["calls"])
    node.children = list(calls)
    return node

def Subcall(identifier):
    node = Node(Subcall, identifier, ["args"])
    node.first = Arguments()
    return node

## function blocks and related node types
def Block():
    node = Node(Block, children_names=["parameters", "body"])
    node.first = Parameters()
    node.second = Body()
    return node

def Body():
    return Node(Body, children_names=["statements"])

def Parameters():
    return Node(Parameters)

def Parameter(name):
    return Node(Parameter, name)

##############################################
# Parsers
##############################################

class ContextlessParser:
    """Base class for contextless parsers, i.e. parsers that can consume tokens
    that can stand alone in an expression or statement (like identifiers or
    constants).
    """

    @classmethod
    def null_parse(cls, parser, token):
        """Parses the token. Responsible for advancing the parser beyond
        the token using parser.advance(), or the .expression()/.statement()
        methods.
        """
        raise ParsingException(
            "null_parse() not implemented properly for %s" % cls.__name__,
            token)

    @classmethod
    def strength(cls, token):
        """Returns how strongly this parser wants to consume the given token.
        In the event of two parsers wanting the same token, the one with the
        higher strength will win.
        """
        return 0

class OperationParser:
    """Base class for operation parsers, i.e. parsers that can consume tokens
    appearing in the middle of an expression, like an operator or a paren.
    """

    @classmethod
    def op_parse(cls, parser, token, left):
        """Parses the token. Responsible for advancing the parser beyond
        the token using parser.advance(), or the .expression()/.statement()
        methods. The subtree representing the left-hand-side argument of the
        operation is passed in.
        """
        raise ParsingException(
            "op_parse() not implemented properly for %s" % cls.__name__,
            token)

    @classmethod
    def op_precedence(cls, token):
        """Returns the precedence this operation should have. Higher values
        result in more tightly binding operations, so '*' will have higher
        precedence than '+'. The default is 50.
        """
        return 50

    @classmethod
    def strength(cls, token):
        """Returns how strongly this parser wants to consume the given token.
        In the event of two parsers wanting the same token, the one with the
        higher strength will win.
        """
        return 0

##############################################
# Individual parsers
##############################################

class IdParser(ContextlessParser):
    """Parses standalone identifiers."""
    TOKENS = [lexer.Id]

    @classmethod
    def null_parse(cls, parser, token):
        parser.advance()
        return Id(token.raw)


class ConstantParser(ContextlessParser):
    """Parses constants of all types."""
    TOKENS = [lexer.IntLiteral, lexer.FloatLiteral, lexer.StrLiteral]

    @classmethod
    def null_parse(cls, parser, token):
        parser.advance()
        return Constant(token.value)


class FunctionCallParser(OperationParser):
    """Parses simple and complex call constructs."""
    TOKENS = ["{", "(", lexer.Id]

    @classmethod
    def op_parse(cls, parser, token, left):
        if token.kind == "{":
            return cls.parse_block(parser, token, left)
        elif token.kind == "(":
            return cls.parse_argument_list(parser, token, left)
        else: # identifier
            return cls.parse_identifier(parser, token, left)

    @classmethod
    def op_precedence(cls, token):
        return 90

    @classmethod
    def parse_block(cls, parser, token, left):
        """Parses calls of type: id { ... }, and more complex versions of a
        code block appearing inline in a call.
        """
        if left.kind == FunctionCall:
            # add block as argument to the call
            returned = host = left
        elif left.kind == ComplexCall:
            # if we are parsing a complex call chain,
            # the block will go as argument to the last subcall
            host = left.children[-1]
            returned = left
        else:
            # make a new call out of it
            returned = host = FunctionCall(left)

        block = BlockParser.null_parse(parser, token)
        host.child("args").add(block)
        return returned

    @classmethod
    def parse_argument_list(cls, parser, _, left):
        """Parses standard argument lists, e.g.: id(a,b,c),
        and more complex variants used in call chains.
        """
        if left.kind == FunctionCall:
            # add arguments to the existing call
            returned = host = left
        elif left.kind == ComplexCall:
            host = left.children[-1]
            returned = left
        else:
            returned = host = FunctionCall(left)

        # empty argument list?
        parser.advance()
        if parser.token.kind == ")":
            parser.advance()
            return returned

        # nope, let's parse the arguments
        while True:
            argument = parser.expression(0)
            host.child("args").children += [argument]
            if parser.token.kind == ")":
                # end of argument list
                parser.advance(")")
                return returned
            else:
                # more arguments have to be here, expect a comma
                parser.advance(",")

    @classmethod
    def parse_identifier(cls, parser, token, left):
        """Parses identifiers used as continuation of a complex call chain."""
        if left.kind == FunctionCall:
            # modify the original call to work in a call chain
            # this requires the original call to have just an identifier as
            # the target
            original_target = left.child("target")
            if original_target.kind != Id:
                parser.error("Illegal complex call - the original call target "
                             "must be a single identifier for complex calls.")
            original_target.value += ".."

            # start a new complex call chain
            parser.advance()
            new_subcall = Subcall(token.raw + "..")
            return ComplexCall(left, new_subcall)
        elif left.kind == ComplexCall:
            # extend the existing chain
            parser.advance()
            left.add(Subcall(token.raw + ".."))
            return left
        else:
            # not a valid position for an identifier
            parser.error("Expected operator or end of statement, " +
                         "not an identifier.")


class BinaryOpParser(OperationParser):
    """Parser for binary operators."""
    TOKENS = [lexer.Operator]

    PRECEDENCE = {
        "=": 10, ":=": 10,
        "==": 20, "!=": 20, "<": 20, ">": 20, "<=": 20, ">=": 20,
        "+": 30, "-": 30,
        "*": 40, "/": 40, "%": 40,
        ".": 95, ":": 95
    }

    @classmethod
    def op_parse(cls, parser, token, left):
        parser.advance()
        precedence = cls.op_precedence(token)
        return BinaryOp(token.raw, left, parser.expression(precedence))

    @classmethod
    def op_precedence(cls, token):
        if token.raw in cls.PRECEDENCE:
            return cls.PRECEDENCE[token.raw]
        elif token.raw[0] in cls.PRECEDENCE:
            return cls.PRECEDENCE[token.raw[0]]
        else:
            return 50


class UnaryOpParser(ContextlessParser):
    """Parser for unary operators."""
    TOKENS = [lexer.Operator]
    PRECEDENCE = 80

    @classmethod
    def null_parse(cls, parser, token):
        parser.advance()
        return UnaryOp(token.raw, parser.expression(cls.PRECEDENCE))

class BlockParser(ContextlessParser):
    """Parser for code blocks."""
    TOKENS = ["|", "{"]

    @classmethod
    def null_parse(cls, parser, token):
        block = Block()
        # do we have an argument list?
        if token.kind == "|":
            # non-empty argument list, read it
            params = Parameters()
            parser.advance()

            while True:
                # is it an id?
                if parser.token.kind != lexer.Id.kind:
                    parser.error("Expected parameter name or '|' but got %s "
                                 "instead." % parser.token)
                # add parameter and advance
                params.add(Parameter(parser.token.value))
                parser.advance()
                # end of list?
                if parser.token.kind == "|":
                    parser.advance()
                    break
                # no, swallow a comma
                parser.advance(",")

            block.first = params

        # read the block body
        parser.advance("{")
        while parser.token.kind != "}":
            statement = parser.statement()
            block.child("body").children.append(statement)
        parser.advance("}")

        return block

class ParenthesisedExpressionParser(ContextlessParser):
    """Parses parenthesised expression like (a + b)."""
    TOKENS = ["("]

    @classmethod
    def null_parse(cls, parser, token):
        parser.advance()
        expression = parser.expression(0)
        parser.advance(")")
        return expression


##############################################
# Parsers list
##############################################

NULL_PARSERS = [IdParser, ConstantParser, BlockParser, UnaryOpParser,
                ParenthesisedExpressionParser]
OP_PARSERS = [BinaryOpParser, FunctionCallParser]

##############################################
# The parsing code itself
##############################################

class Parser:
    """The actual parser in a class."""

    LEFT = "left"
    NULL = "null"

    def __init__(self, null_parsers, op_parsers):
        self.null_parsers = self._create_parser_map(null_parsers)
        self.op_parsers = self._create_parser_map(op_parsers)

    def _create_parser_map(self, parsers):
        pm = {}
        for parser in parsers:
            for token_kind in parser.TOKENS:
                if not isinstance(token_kind, str):
                    token_kind = token_kind.kind
                pm[token_kind] = pm.get(token_kind, []) + [parser]
        return pm

    def parse(self, tokens):
        """Parses a list of tokens into an abstract syntax tree of Node
        objects.
        """
        self.tokens = tokens

        module_body = Body()
        while self.tokens:
            module_body.children.append(self.statement())

        return module_body

    @property
    def token(self):
        """The token currently being processed."""
        if len(self.tokens) > 0:
            return self.tokens[0]
        else:
            return None

    @property
    def next(self):
        """The upcoming token after the current one."""
        if len(self.tokens) > 1:
            return self.tokens[1]
        else:
            return None

    def null_parser(self, token):
        """Picks the best parser for a given token when we have no current
        context.
        """
        return self.pick_parser(self.null_parsers, token)

    def op_parser(self, token):
        """Picks the best parser for a given token when we have a
        left-hand-side context and expect an operator.
        """
        return self.pick_parser(self.op_parsers, token)

    def pick_parser(self, parsers, token):
        try:
            collection = parsers[token.kind]
            if len(collection) == 1:
                return collection[0]
            else:
                # multiple parsers, get the one with the best strength
                ordered = sorted(collection, key=lambda p:-p.strength(token))
                return ordered[0]

        except KeyError:
            # no match, no parser
            return None

    def expect(self, token_kind):
        """Asserts that the current token matches the kind given. Anything
        else will trigger a parse error.
        """
        token = self.token
        if token is None:
            self.error(
                "Unexpected end of file, expected %s instead." % token_kind)
        if token.kind != token_kind:
            self.error("Expected %s but got %s instead." % (token_kind, token))

    def advance(self, expected=None):
        """Advance one token ahead in the stream. Optionally,
        you can provide the expected kind of token to find in that place -
        any other token will trigger a parse error.
        """
        if expected:
            self.expect(expected)
        self.tokens = self.tokens[1:]
        return self.token


    def expression(self, min_precedence):
        """Parses an entire expression starting at the current position. The
        expression will terminate when there is no more operators,
        or the next operator has precedence lower than the minimum specified.
        This ensures correct operator nesting.
        """

        # read the first term
        first_term_parser = self.null_parser(self.token)
        if not first_term_parser:
            self.error("This token cannot start an expression.")
        expression = first_term_parser.null_parse(self, self.token)

        # and the following terms
        while True:
            if not self.token:
                # end of stream, nothing more to parse
                return expression
            parser = self.op_parser(self.token)
            if not parser or parser.op_precedence(self.token) <= min_precedence:
                # either there is no way to parse the expression further or
                # the operator precedence is too low, this expression will
                # become the left-side to the following operator
                return expression
            expression = parser.op_parse(self, self.token, expression)

    def statement(self):
        """Parses a statement, which is just a terminated expression."""
        expression = self.expression(0)
        self.advance(";")
        return expression

    def error(self, text, token = None):
        """Raises a parsing error, either from the current token,
        or the token given as parameter.
        """
        if not token:
            token = self.token
        raise ParsingException(text, token)


def parse(tokens):
    """Sets up the parser with the default configuration and parses the given
     stream of tokens.
    """
    parser = Parser(NULL_PARSERS, OP_PARSERS)
    return parser.parse(tokens)
