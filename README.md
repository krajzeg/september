# September 0.1

September is, or rather will be a dynamic, curly-brace programming language focused on minimalism. The idea is to strive for the extensibility of Lisp with the straightforwardness of Python. 

## The general idea

The September way is to make everything a method call, even things like `if`, `while` or `try..catch`. The function call syntax of the  language is adaptable to the point that a `try..catch..finally` invocation can look exactly the same as it would if it was all built-in keywords. The overarching idea behind that is that by implementing core features that way, we can achieve the best extensibility, ensuring that users of the language will be able to add new constructs that feel as natural as if they were part of the core syntax.

## Current status

This is a **very early version** of the language, and the 0.1 version number does not lie. The parser/compiler is a separate piece of code written in Python and compiles down to a simple bytecode format. The interpreter for this bytecode is written in C. Many basic capabilities of the language (such as defining functions that take parameters) are not yet in place, and the runtime support is minimal (the Bool type doesn't support any operators yet!). **Everything is experimental** at this point, and there are some parts of the codebase (such as the Python parser) that are intended to be completely thrown out at one point. So, I guess what I want to say is: *caveat emptor*.

## Still want to try it?

To try it out, build the interpreter, then compile one of the examples, and run it!

    cd c
    make
	cd ../py
    python sepcompiler.py ../tests/<oneoftheexamples>.sep
	cd ../c
    ./09 ../tests/<oneoftheexamples>.09

Your `python` should be Python 3.x.
