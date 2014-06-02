# September 0.2

September is a dynamic, curly-brace programming language focused on minimalism. The idea is to strive for the extensibility of Lisp with the straightforwardness of Python. 

## The general idea

The September way is to make everything a function call, including things like `if`, `return` or `try..catch`. The function call syntax of the language is designed for it, so a `try..catch..finally` invocation can look exactly the same as it would if it was all built-in keywords. The overarching idea behind that is that by implementing core languages features in this way, we can ensure that users of the language will be able to add new constructs that feel natural.

## Current status

This is a **very early version** of the language. The experiment seems to be successful so far, and most of the features of the virtual machine and interpreter are complete as of version 0.2.

The road to version 0.3 is all about getting September from "experimental language based on sound principles" to "useful language". Here is a high-level roadmap for doing just that:

* ✓ use [**C3 linearization**](http://en.wikipedia.org/wiki/C3_linearization) for property resolution
* equip each of the **built-in types** (Integer, String, etc.) with the expected methods and operators, including array slicing
* implement **a compiler for September in September itself**, allowing us to ditch the Python part of the codebase, and making September a bootstrapped language
* create a workable **module/import system** based on that compiler
* create a **September [REPL](http://en.wikipedia.org/wiki/REPL)**
* provide usable **exception backtraces**
* finalize the **class system** by adding the following features: **instance/static fields/methods**, **constructors**, **inheritance and mixins**
* create a **minimal standard I/O library** allowing September to interact with the outside world through **standard in/out**, **files** and **sockets**

## Want to try it?

In this early version there is not much in terms of documentation, but you can still try it out by:

1. Make sure you have **a GCC-compatible C compiler** (MinGW on Windows, or any gcc-like on Linux or Mac) and **Python 3.x** installed. Python 3.x has to be the
   default Python, otherwise the build won't work.
2. Clone the repository: `hg clone https://krajzega@bitbucket.org/krajzega/september`
3. Switch to the stable branch to avoid any brokenness happenning on the default branch: `hg up stable`
4. Build September: `make`
5. Run any September code you want using the `run` scripts: `./run tests/<anyofthetests>.09`.

## Read more

More information about the language can be found on my [blog](http://wasyl.eu/tags/september/).
