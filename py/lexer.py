import re

class Token:
	def __init__(self, kind, raw):
		self.kind = kind
		self.raw = raw

	def __str__(self):
		if hasattr(self, "value"):
			return "%s'%s'" % (self.kind, self.value)
		else:
			return "%s'%s'" % (self.kind, self.raw)

	def __repr__(self):
		return "[%s]" % str(self)


class Id(Token):
	KIND = "id"
	def __init__(self, raw):
		super().__init__(self.__class__.KIND, raw)

class IntLiteral(Token):
	KIND = "int"
	def __init__(self, raw):
		super().__init__(self.__class__.KIND, raw)
		self.value = int(raw)

class FloatLiteral(Token):
	KIND = "float"
	def __init__(self, raw):
		super().__init__(self.__class__.KIND, raw)
		self.value = float(raw)

class StrLiteral(Token):
	KIND = "str"
	PAIRINGS = {
		'\\\\': '\\',
		'\\n':  '\n',
		'\\"':  '\"'
	}

	def __init__(self, raw):
		string = raw[1:-1]
		for before, after in StrLiteral.PAIRINGS.items():
			string = string.replace(before, after)
		self.value = string
		super().__init__(self.__class__.KIND, raw)

class Operator(Token):
	KIND = "binary"

	def __init__(self, raw):
		super().__init__(self.__class__.KIND, raw)

def Simple(kind):
	class SimpleToken(Token):
		def __str__(self):
			return self.kind
	def factory(raw):
		return SimpleToken(kind, raw)
	return factory


TOKENS = {
	r"[_a-zA-Z][a-zA-Z0-9]*":     Id,
	r"[0-9]+":                    IntLiteral,
	r"[0-9]+\.[0-9]+":            FloatLiteral,
	r'"([^"\\]|\\\\")*([^\\])?"': StrLiteral,
	r'[+*/:.!<>=-]+|`[a-z]+':     Operator,
	r'\(':						  Simple("("),
	r'\)':						  Simple(")"),
	r'\{':                        Simple("{"),
	r'\}':                        Simple("}"),
	r',':                         Simple(","),
	r';':                         Simple(";")
}

WHITES = r"\s+"

class Lexer:
	def __init__(self, token_map, white_exp):
		self.tokens = {re.compile(exp): cls for exp,cls in token_map.items()}
		self.whitespace = re.compile(white_exp)

	def lex(self, input):
		# initial parameters
		self.stream = input
		self.line = 1
		self.column = 1
		results = []

		# scan all the input, if possible
		while self.stream != "":
			# eat any whitespace
			white_match = self.whitespace.match(self.stream)
			if white_match:
				self.consume(white_match.group(0))

			# did we eat it all?
			if self.stream == "":
				break
			
			# find longest token match
			longest = (None, 0)
			for rexp, token_class in self.tokens.items():
				match = rexp.match(self.stream)
				if match and len(match.group(0)) > longest[1]:
					longest = (token_class(match.group(0)), len(match.group(0)))

			# did we fail to find anything?
			token, length = longest
			if length == 0:
				self.error("unrecognized input: '%s'" % self.rest_of_line())
				return None

			# nope
			results.append(token)
			self.consume(token.raw)

		return results

	def consume(self, string):
		for char in string:
			if char == '\n':
				self.line += 1
				self.column = 1
			else:
				self.column += 1
		self.stream = self.stream[len(string):]

	def error(self, text):
		raise Exception("Parsing error at [line %d:col %d]: %s" % (self.line, self.column, text))

	def rest_of_line(self):
		try:
			new_line_pos = self.stream.index("\n")
			return self.stream[:new_line_pos]
		except ValueError:
			return self.stream



def lex(string):
	lexer = Lexer(TOKENS, WHITES)
	return lexer.lex(string)
