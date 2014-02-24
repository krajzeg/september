import lexer

class ParsingError(Exception):
    def __init__(self, message, token):
        self.token = token
        if token:
            message = "[line %d,col %d]: %s" % (token.location[0], token.location[1], message)
        super().__init__(message)

class Node:
    KIND = "<unknown>"

    def __init__(self, value = None):
        self.value = value
        self.children = []

    @staticmethod
    def null_parse(parser, token):
        parser.error("%s is not expected to be used at the beginning of an expression." % token.kind, token)

    @staticmethod
    def left_parse(parser, token, left):
        parser.error("%s is not expected to be used as an operator." % token.kind, token)

    @staticmethod
    def binding_power(token):
        return -1

    @staticmethod
    def left_preference():
        return -1
    
    @staticmethod
    def null_preference():
        return -1

    @staticmethod
    def error(text):
        raise ParsingError(text)

    @property
    def kind(self):
        return self.__class__.KIND

    def child(self, index):
        if len(self.children) > index:
            return self.children[index]
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

def SimpleNode(kind):
    class Simple(Node):
        KIND = kind
        def __init__(self):
            super().__init__()
    return Simple


class ComplexCall(Node):
    KIND = "complexcall"
    EXECUTOR_NAME = "..!"

    def __init__(self, *calls):
        super().__init__()
        self.children = list(calls)
        self.value = ComplexCall.EXECUTOR_NAME

class Id(Node):
    TOKENS = [lexer.Id.KIND]
    KIND = "id"

    def __init__(self, name):
        super().__init__(name)

    @staticmethod
    def null_parse(parser, token):
        parser.advance()
        return Id(token.raw)

    def left_parse(parser, token, left):
        # swallow the id
        parser.advance()

        if left.kind == FCall.KIND:
            if left.first.kind == Id.KIND:
                # change the original identifier to include '..'
                original_id = left.first
                original_id.value += ".."

                # embed original call in a new complex call
                subcall = Subcall(token.raw + "..")
                return ComplexCall(left, subcall)
            else:
                parser.error("Identifier '%s' encountered in a place where it makes no sense." % token.raw, token)
        elif left.kind == ComplexCall.KIND:
            # add a new subcall
            left.children += [Subcall(token.raw + "..")]
            return left
        else:
            parser.error("Identifier '%s' encountered in a place where it makes no sense." % token.raw, token)

    @staticmethod
    def binding_power(token):
        return 90

class UnaryOp(Node):
    TOKENS = [lexer.Operator.KIND]
    KIND = "unary"
    PRECEDENCE = 80

    def __init__(self, operator):
        super().__init__("unary" + operator)

    @staticmethod
    def null_parse(parser, token):
        op = UnaryOp(token.raw)
        parser.advance()

        op.first = parser.expression(UnaryOp.PRECEDENCE)
        return op

    @staticmethod
    def null_preference():
        return 10

class BinaryOp(Node):
    TOKENS = [lexer.Operator.KIND]
    KIND = "binary"
    PRECEDENCE = {
        "=": 10,
        "==": 20, "!=": 20, "<": 20, ">": 20, "<=": 20, ">=": 20,
        "+": 30, "-": 30,
        "*": 40, "/": 40, "%": 40,
        ".": 95, ":": 95
    }

    def __init__(self, operator):
        super().__init__(operator)

    @staticmethod
    def left_parse(parser, token, left):
        op = BinaryOp(token.raw)
        op.first = left

        parser.advance()
        op.second = parser.expression(BinaryOp.binding_power(token))

        return op

    @staticmethod
    def binding_power(token):
        first = token.raw[0]
        if token.raw in BinaryOp.PRECEDENCE:
            return BinaryOp.PRECEDENCE[token.raw]
        elif first in BinaryOp.PRECEDENCE:
            return BinaryOp.PRECEDENCE[first]
        else:
            return 0

    @staticmethod
    def left_preference():
        return 10

FArgs = SimpleNode("fargs")

class FCall(Node):
    KIND = "fcall"

    def __init__(self, left):
        super().__init__()
        self.first = left
        self.second = FArgs()

    @property
    def args(self):
        return self.second

class Subcall(Node):
    KIND = "subcall"

    def __init__(self, method_name):
        super().__init__(method_name)
        self.first = FArgs()
    
    @property
    def args(self):
        return self.children[0]

class ParenParser:
    TOKENS = ["("]

    @staticmethod
    def left_parse(parser, token, left):
        if left.kind == FCall.KIND:
            # extend existing call
            final = call = left
        if left.kind == ComplexCall.KIND:
            # extend last call in the complex
            call = left.children[-1]
            final = left
        else:
            # new call
            final = call = FCall(left)
            
        parser.advance()

        # zero-arg call
        if parser.token.kind == ")":
            parser.advance()
            return call

        while True:
            arg = parser.expression(0)
            call.args.children += [arg]
            if parser.token.kind == ")":
                parser.advance()
                return final
            else:
                parser.advance(",")

    @staticmethod
    def null_parse(parser, token):
        parser.advance()
        expression = parser.expression(0)
        parser.advance(")")
        return expression

    @staticmethod
    def binding_power(token):
        return 90

class Block(Node):
    TOKENS = ["{"]
    KIND = "block"

    def __init__(self):
        super().__init__()
        self.first = Body()

    @staticmethod
    def left_parse(parser, token, left):
        if left.kind == FCall.KIND:
            final = call = left        
        elif left.kind == ComplexCall.KIND:
            call = left.children[-1]
            final = left
        else:
            final = call = FCall(left)

        call.args.children += [Block.null_parse(parser, token)]

        return final

    @staticmethod
    def null_parse(parser, token):
        parser.advance()
        block = Block()
        while (parser.token.kind != "}"):
            stmt = parser.statement()
            block.body.add(stmt)
        parser.advance("}")

        return block

    @staticmethod
    def binding_power(token):
        return 90

    @property
    def body(self):
        return self.first

class Constant(Node):
    KIND = "const"
    TOKENS = [lexer.IntLiteral.KIND, lexer.FloatLiteral.KIND, lexer.StrLiteral.KIND]

    def __init__(self, value):
        super().__init__(value)

    @staticmethod
    def null_parse(parser, token):
        parser.advance()
        return Constant(token.value)

class Body(Node):
    KIND = "body"

    def __init__(self):
        super().__init__()

    def add(self, stmt):
        self.children += [stmt]


PARSER_CLASSES = [Id, UnaryOp, BinaryOp, ParenParser, Block, Constant]
PARSERS = {}
for parser_class in PARSER_CLASSES:
    for token in parser_class.TOKENS:
        PARSERS[token] = PARSERS.get(token, []) + [parser_class]

class Parser:
    LEFT = "left"
    NULL = "null"

    def __init__(self, parsers):
        self.parsers = parsers

    def parse(self, tokens):
        self.tokens = tokens

        stmts = Body()
        while self.tokens:
            stmts.add(self.statement())

        return stmts

    @property
    def token(self):
        if len(self.tokens) > 0:
            return self.tokens[0]
        else:
            return None

    @property
    def next(self):
        if len(self.tokens) > 1:
            return self.tokens[1]
        else:
            return None

    def factory(self, token, context):
        try:
            collection = self.parsers[token.kind]
            if len(collection) == 1:
                return collection[0]

            # decide among many possible - they will have preferences
            if context == Parser.LEFT:
                return sorted(collection, key = lambda x: -x.left_preference())[0]
            if context == Parser.NULL:
                return sorted(collection, key = lambda x: -x.null_preference())[0]

            raise ParsingError("Unable to decide how to parse token: %s" % token)

        except KeyError:
            return None

    def expect(self, token_kind):
        if self.token is None:
            self.error("Unexpected end of file, expected %s instead." % token_kind)
        if self.token.kind != token_kind:
            self.error("Expected %s, got %s('%s') instead." % (token_kind, self.token.kind, self.token), self.token)

    def advance(self, expected = None):
        if expected:
            self.expect(expected)
        self.tokens = self.tokens[1:]
        return self.token


    def expression(self, min_bp):
        first_fact = self.factory(self.token, Parser.NULL)
        if first_fact is None:
            self.error("Incorrect token used at beginning of expression: %s." % self.token.raw)
        expr = first_fact.null_parse(self, self.token)

        while self.token and self.factory(self.token, Parser.LEFT) and self.factory(self.token, Parser.LEFT).binding_power(self.token) > min_bp:
            factory = self.factory(self.token, Parser.LEFT)
            expr = factory.left_parse(self, self.token, expr)

        return expr

    def statement(self):
        expr = self.expression(0)
        self.advance(";")
        return expr

    def error(self, text, token = None):
        raise ParsingError(text, token)



def parse(tokens):
    parser = Parser(PARSERS)
    return parser.parse(tokens)
