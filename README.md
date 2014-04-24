# September 0.1

September is, or rather will be a dynamic, curly-brace programming language focused on minimalism. The idea is to strive for the extensibility of Lisp with the straightforwardness of Python. 

## The general idea

The September way is to make everything a method call, even things like `if`, `while` or `try..catch`. The function call syntax of the  language is adaptable to the point that a `try..catch..finally` invocation can look exactly the same as it would if it was all built-in keywords. The overarching idea behind that is that by implementing core features that way, we can achieve the best extensibility, ensuring that users of the language will be able to add new constructs that feel as natural as if they were part of the core syntax.

## Current status

This is a **very early version** of the language. From the work done so far, it seems that the no-keyword experiment was successful and the resulting language looks promising. 

The majority of the effort right now goes to making the virtual machine fully featured, leaving the standard library for later. This is the roadmap to version 0.2:

* ☑ implement a simple mark-and-sweep garbage collector 
* ☑ implement all basic features related to functions and function calls
	* ☑ "sink" arguments (`|...things|`)
	* ☑ optional arguments (`|reverse = False|`)
	* ☑ named arguments in calls (`people.sort(reverse: True)`) 
* ☑ array/object "literals" (`[1, 2, 3]`, `[[x:1, y:2]]`)
* ☑ indexing operator (`a[2]`)
* ☑ normalize syntax so statements like `return a+b;`, `return;`, `break;` and `continue;` all work as expected
* ☐ allow for writing and importing modules
* ☐ get the basic class system with constructors, fields and methods working

Once those things are done, the next step will be making the language capable of implementing its own parser/compiler by writing the most basic parts of the standard library.

## Want to try it?

In this early version there is not much in terms of documentation - but you can still try it out!
Just build the interpreter, pick one of the examples in the "tests" directory, and run it.

    make
    ./run tests/<oneoftheexamples>.sep   # or just 'run' on Windows

If you're building under Linux/Unix, you need to have **Python 3.x** installed, and it has to be available under the name `python3` on your path.

If you want to build under Windows, you will need both **Python 3.x** (as the default `python` executable) and **MinGW** to compile the interpreter.

## Read more

More information about the language can be found on my [blog](http://wasyl.eu/tags/september/), where I intend to post rationales, updates and generally interesting stuff on the language.
