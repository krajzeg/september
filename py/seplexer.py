##########################################################################
#
# seplexer
#
# This package implements a lexer for September. The lexer is very simple,
# based on a couple of regular expressions and always proceeding with the
# longest match.
#
##########################################################################

import re

##############################################
# Exceptions
##############################################

class LexerException(Exception):
    """Represents errors encountered during the lexing step of compilation,
    mostly malformed input.
    """
    def __init__(self, message, location):
        super().__init__(message)
        self.location = location

##############################################
# Tokens
##############################################

class Token:
    """Represents a single token lexed from the input."""

    def __init__(self, kind, raw_text):
        self.kind = kind
        self.raw = raw_text
        self.value = raw_text

    def is_a(self, token_type):
        if isinstance(token_type, str):
            return self.kind == token_type
        else:
            return self.kind == token_type.kind

    def __str__(self):
        if self.kind == self.raw:
            return self.raw  # simple token like '(' or ','
        elif hasattr(self, "value"):
            return "%s:'%s'" % (self.kind, self.value)
        else:
            return "%s:'%s'" % (self.kind, self.raw)

    def __repr__(self):
        return "[%s]" % str(self)

def token_type(tag, initialization = None):
    """Defines a new token type by creating a factory
    function for it.
    """
    def token_factory(raw_text):
        token = Token(tag, raw_text)
        if initialization:
            initialization(token)
        return token

    # remember the token tag inside the factory
    token_factory.kind = tag
    return token_factory

##############################################
# Individual token types
##############################################

def int_literal_init(token):
    token.value = int(token.raw)

def float_literal_init(token):
    token.value = float(token.raw)

STRING_ESCAPES = {
    r"\\": "\\",
    r"\n": "\n",
    r'\"':  '"'
}
def str_literal_init(token):
    string = token.raw[1:-1] # extract the relevant part
    for escape_sequence, result in STRING_ESCAPES.items():
        string = string.replace(escape_sequence, result)
    token.value = string

Id = token_type("id")
Operator = token_type("operator")
IntLiteral = token_type("int", int_literal_init)
FloatLiteral = token_type("float", float_literal_init)
StrLiteral = token_type("str", str_literal_init)

##############################################
# Regex mappings
##############################################

TOKENS = {
    r"[_a-zA-Z][a-zA-Z0-9_]*":    Id,
    r"[0-9]+":                    IntLiteral,
    r"[0-9]+\.[0-9]+":            FloatLiteral,
    r'"([^"\\]|\\\\")*([^\\])?"': StrLiteral,
    r'[?+*/%:.!<>=-]+':           Operator,
    r'\(':                        token_type("("),
    r'\)':                        token_type(")"),
    r'\{':                        token_type("{"),
    r'\}':                        token_type("}"),
    r',':                         token_type(","),
    r';':                         token_type(";"),
    r'\|':                        token_type("|")
}

WHITESPACE = r"\s+"

##############################################
# The lexer code
##############################################

class Lexer:
    """Implements the actual lexing of an input string."""

    def __init__(self, token_map, white_exp):
        self.tokens = {re.compile(exp): cls for exp,cls in token_map.items()}
        self.whitespace = re.compile(white_exp)

    def lex(self, input_string):
        """Lexes the string provided as input and returns a list of tokens."""

        # initial state
        self.stream = input_string
        self.line = 1
        self.column = 1
        results = []

        # scan all the input, if possible
        while self.stream != "":
            # eat any whitespace first
            white_match = self.whitespace.match(self.stream)
            if white_match:
                self.consume(white_match.group(0))

            # did we run out of stream?
            if self.stream == "":
                break

            # find the longest match
            longest = (None, 0)
            for rexp, token_class in self.tokens.items():
                match = rexp.match(self.stream)
                if match and len(match.group(0)) > longest[1]:
                    longest = (token_class(match.group(0)), len(match.group(0)))

            # did we fail to find a suitable match?
            token, length = longest
            if length == 0:
                self.error("Unrecognized input: '%s'" % self.rest_of_line())

            # store the location we got the token from and append it
            token.location = (self.line, self.column)
            results.append(token)

            # consume the text
            self.consume(token.raw)

        return results

    def consume(self, string):
        """Consumes the provided string, advancing line and column
        counters in the process.
        """
        for char in string:
            if char == '\n':
                self.line += 1
                self.column = 1
            else:
                self.column += 1
        self.stream = self.stream[len(string):]

    def error(self, text):
        """Streamlines raising lexing problems."""
        raise LexerException(text, (self.line, self.column))

    def rest_of_line(self):
        """Returns the remaining un-lexed part of the current line."""
        try:
            new_line_pos = self.stream.index("\n")
            return self.stream[:new_line_pos]
        except ValueError:
            return self.stream

def lex(string):
    """Creates a new lexer instance using the standard tables and
    lexes the provided string."""
    lexer = Lexer(TOKENS, WHITESPACE)
    return lexer.lex(string)
