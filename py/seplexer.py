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

def bracket_init(token):
    closing = token.value.translate(str.maketrans("({[<", ")}]>"))[::-1]
    token.counterpart = CloseBracket(closing)

Id = token_type("id")
Operator = token_type("operator")
IntLiteral = token_type("int", int_literal_init)
FloatLiteral = token_type("float", float_literal_init)
StrLiteral = token_type("str", str_literal_init)
StatementEnd = token_type(";")
Comment = token_type("comment")
EndOfFile = token_type("end of file")
OpenBracket = token_type("openbracket", bracket_init)
CloseBracket = token_type("closebracket")

##############################################
# Regex mappings
##############################################

TOKENS = {
    r"[_a-zA-Z][a-zA-Z0-9_]*":    Id,
    r"[0-9]+":                    IntLiteral,
    r"[0-9]+\.[0-9]+":            FloatLiteral,
    r'"([^"\\]|\\\\")*([^\\])?"': StrLiteral,
    r';':                         StatementEnd,
    r'\[[[{<]*':                  OpenBracket,
    r'[}>\]]*]':                  CloseBracket,

    r':':                         token_type(":"),
    r'\(':                        token_type("("),
    r'\)':                        token_type(")"),
    r'\{':                        token_type("{"),
    r'\}':                        token_type("}"),
    r',':                         token_type(","),
    r'\|':                        token_type("|"),

    r'[?&^+*/%.!<>=-][:?&^+*/%.!<>=-]*|:[:?&^+*/%.!<>=-]+|\|\|': Operator,

    r'#.*':                       Comment
}

WHITESPACE = r"\s+"

# noinspection PyUnresolvedReferences
ASI_STATEMENT_ENDING_TOKENS = [
    Id.kind, IntLiteral.kind, FloatLiteral.kind, StrLiteral.kind, CloseBracket.kind,
    ")", "}"
]
ASI_TRIGGER_TOKENS = ["}"]
# noinspection PyUnresolvedReferences
ASI_DISABLING_TOKENS = [Operator.kind]

# noinspection PyUnresolvedReferences
OPEN_TOKENS = ["(", "{", OpenBracket.kind]
# noinspection PyUnresolvedReferences
CLOSE_TOKENS = [")", "}", CloseBracket.kind]

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
        self.results = []
        self.bracket_context = []

        # scan all the input, if possible
        while self.stream != "":
            asi_triggered = False
            # eat any whitespace first
            white_match = self.whitespace.match(self.stream)
            if white_match:
                # new-lines trigger semicolon insertion
                if "\n" in white_match.group(0):
                    asi_triggered = True
                # eat the whitespace
                self.consume(white_match.group(0))

            # did we run out of stream?
            if self.stream == "":
                break

            # find the longest match
            token = self.longest_matching_token()
            if token is None:
                self.error("Unrecognized input: '%s'" % self.rest_of_line())

            # before we store the token, we check for semicolon insertion
            if token.kind in ASI_TRIGGER_TOKENS or asi_triggered:
                self.inject_semicolon_if_needed(token)

            # update the bracket context
            if token.kind in OPEN_TOKENS:
                self.bracket_context.append(token.value)
            elif token.kind in CLOSE_TOKENS:
                # check for "matchedness" of brackets
                if not self.bracket_context:
                    self.error("Parenthesis/bracket closed, but it was never opened.")
                expected_closer = self.matching_closer(self.bracket_context[-1])
                if token.value != expected_closer:
                    self.error("Mismatched brackets: expected %s, got %s." % (expected_closer, token.value))
                # everything in order
                self.bracket_context.pop()

            # append the token and remove it from the character stream
            self.append(token)
            self.consume(token.raw)


        # one more possible ASI at end of file
        self.inject_semicolon_if_needed(EndOfFile(""))

        return self.results


    def longest_matching_token(self):
        longest_token, best_length = None, 0

        for rexp, token_class in self.tokens.items():
            match = rexp.match(self.stream)
            if match and len(match.group(0)) > best_length:
                text = match.group(0)
                longest_token = token_class(text)
                best_length = len(text)

        return longest_token


    def rest_of_line(self):
        """Returns the remaining un-lexed part of the current line."""
        try:
            new_line_pos = self.stream.index("\n")
            return self.stream[:new_line_pos]
        except ValueError:
            return self.stream


    def append(self, token):
        """Appends a token to the token stream."""
        if not token.is_a(Comment):
            token.location = (self.line, self.column)
            self.results.append(token)

    def inject_semicolon_if_needed(self, following_token):
        """Handles automatic semicolon insertion. If the last token was
        something that could end a statement, and the next token is
        something that could reasonably start a statement this method injects
        an implicit semicolon into the token stream.
        """
        if not self.results:
            return
        if not self.results[-1].kind in ASI_STATEMENT_ENDING_TOKENS:
            return
        if following_token.kind in ASI_DISABLING_TOKENS:
            return
        if self.bracket_context and self.bracket_context[-1] != "{":
            return
        self.append(StatementEnd(";"))


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
        """Raises a lexing exception with the current line-column position."""
        raise LexerException(text, (self.line, self.column))


    @staticmethod
    def matching_closer(opening):
        return opening.translate(str.maketrans("([{<", ")]}>"))[::-1]



def lex(string):
    """Creates a new lexer instance using the standard tables and
    lexes the provided string."""
    lexer = Lexer(TOKENS, WHITESPACE)
    return lexer.lex(string)
