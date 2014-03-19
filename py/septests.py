#!/usr/bin/env python3

##########################################################################
#
# septests
#
# Test runner for September test cases. Test cases are simply September
# programs to compile and run. Their output is then checked against a
# "blessed" output from a previous run to detect regressions.
#
##########################################################################

import subprocess
import os
import os.path as path
import sys

##############################################
# Possible test results
##############################################

class TestResult:
    """Represents the result of running a test."""
    SUCCESS, WARNING, FAILURE = range(3)

    def __init__(self, text, status):
        self.text = text
        self.status = status

    def __str__(self):
        if self.status == TestResult.SUCCESS:
            return Ansi.in_color(self.text, Ansi.WHITE)
        elif self.status == TestResult.FAILURE:
            return Ansi.in_color(self.text, Ansi.RED)
        elif self.status == TestResult.WARNING:
            return Ansi.in_color(self.text, Ansi.YELLOW)
        return self.text


OK = TestResult("ok", TestResult.SUCCESS)
NEW_OUTPUT = TestResult("new output", TestResult.SUCCESS)

FIRST_RUN = TestResult("first run", TestResult.WARNING)

WRONG_OUTPUT = TestResult("wrong output", TestResult.FAILURE)
COMPILATION_FAILED = TestResult("compilation failed", TestResult.FAILURE)
EXECUTION_FAILED = TestResult("execution failed", TestResult.FAILURE)

##############################################
# Test file class
##############################################

class TestFile:
    """Represents a single test and is responsible for running it to
    conclusion.
    """

    def __init__(self, root_directory, filename):
        """Creates a new test based on a .sep file."""
        self.source_name = filename
        extensionless, _ = path.splitext(filename)
        self.binary_name = extensionless + ".09"
        self.name = path.relpath(extensionless, root_directory)

    def run(self, distribution_root):
        """Runs the test, returns the proper TestResult,
        and stores it for later reference."""
        self.result = self._compile_and_execute(distribution_root)
        return self.result

    def _compile_and_execute(self, distribution_root):
        """Runs the test and returns the proper TestResult."""

        # compile the script
        compiler = path.join(distribution_root, "py/sepcompiler.py")
        compilation_error = subprocess.call([sys.executable, compiler,
                                             self.source_name,
                                             self.binary_name])
        if compilation_error:
            return COMPILATION_FAILED

        # execute the script
        try:
            interpreter = path.join(distribution_root, "c/09.exe")
            if not path.exists(interpreter):
                interpreter = path.join(distribution_root, "c/09")
            if not path.exists(interpreter):
                sys.stderr.write("Interpreter not found.\n")
                sys.exit(2)

            output = subprocess.check_output([interpreter, self.binary_name])
            output = output.decode(sys.getdefaultencoding())
            output = output.replace("\r\n", "\n")
        except subprocess.CalledProcessError:
            return EXECUTION_FAILED

        # store output for later
        self.output = output

        # check the output for correctness
        expected = self.expected_output()
        if expected is None:
            self.write_actual_output(output)
            return FIRST_RUN
        elif expected != output:
            self.write_actual_output(output)
            return WRONG_OUTPUT
        else:
            return OK

    @property
    def failed(self):
        return self.result.status == TestResult.FAILURE

    def clean(self):
        """Cleans left-over files from previous runs."""
        actual_output_file = path.splitext(self.source_name)[0] + ".actual"
        if path.exists(self.binary_name):
            os.unlink(self.binary_name)
        if path.exists(actual_output_file):
            os.unlink(actual_output_file)

    def bless_output(self):
        """Blesses the output from the last run of this test,
        making it the expected output.
        """
        actual_output_file = path.splitext(self.source_name)[0] + ".actual"
        expected_output_file = path.splitext(self.source_name)[0] + ".expected"
        if path.exists(expected_output_file):
            os.unlink(expected_output_file)
        os.rename(actual_output_file, expected_output_file)

    def write_actual_output(self, output):
        """Writes down the actual output of the run."""
        actual_output_file = path.splitext(self.source_name)[0] + ".actual"
        with open(actual_output_file, "w") as f:
            f.write(output)

    def expected_output(self):
        """Fetches the expected output (if any)."""
        expected_output_file = path.splitext(self.source_name)[0] + ".expected"
        if not path.exists(expected_output_file):
            return None
        else:
            with open(expected_output_file, "r", encoding="utf8") as f:
                return f.read()

##############################################
# ANSI controls
##############################################

class Ansi:
    BLACK, RED, GREEN, BROWN, BLUE, MAGENTA, CYAN, GRAY = range(8)
    DARK_GRAY, LIGHT_RED, LIGHT_GREEN, YELLOW, LIGHT_BLUE, LIGHT_MAGENTA, \
    LIGHT_CYAN, WHITE = range(8,16)

    start = "\x1b["
    enabled = True

    @classmethod
    def sequence(cls, text):
        return cls.start + text if cls.enabled else ""

    @classmethod
    def clear_screen(cls):
        return cls.sequence("2J")

    @classmethod
    def fg(cls, which):
        string = cls.reset()
        if which > 8:
            string += cls.sequence("1m")
            which -= 8
        string += cls.sequence("3%sm" % which)
        return string

    @classmethod
    def bg(cls, which):
        return cls.sequence("4%sm" % which)

    @classmethod
    def reset(cls):
        return cls.sequence("0m")

    @classmethod
    def in_color(cls, text, fg = 7, bg = 0):
        sequence = cls.reset()
        if fg != 7:
            sequence += cls.fg(fg)
        if bg != 0:
            sequence += cls.bg(bg)
        return sequence + text + cls.reset()

##############################################
# Phases of execution
##############################################

def gather_test_files(file_args):
    """Gather all the test files to be run based on the root directory passed
     by the user."""
    for dir_or_file in file_args:
        if path.isdir(dir_or_file):
            for current_dir, _, files in os.walk(dir_or_file):
                for file in filter(lambda f: f.endswith(".sep"), files):
                    yield TestFile(dir_or_file, path.join(current_dir, file))
        else:
            yield TestFile(path.dirname(dir_or_file), dir_or_file)

def clean_leftovers(tests):
    """Cleans left-over files from previous executions."""
    for test in tests:
        test.clean()


def ask_for_blessing(test):
    """Asks the user to bless the output of the test, and blesses it if the
    user chooses so.
    """
    print()
    msg = "Is this output correct ([Y]es/[N]o/[S]top)? "
    print(Ansi.in_color(msg, Ansi.WHITE), end="")
    choice = input().lower()
    if choice.startswith("y"):
        test.bless_output()
        test.result = NEW_OUTPUT
    elif choice.startswith("s"):
        sys.exit(2)
    else:
        test.result = WRONG_OUTPUT

def check_new_test(test):
    """Asks the user to bless a new test."""
    msg = "Test '%s' was run for the first time with the following output:\n"
    print(Ansi.clear_screen())
    print(Ansi.in_color(msg % test.name, Ansi.WHITE))

    for line in test.output.split("\n")[:-1]:
        print("  ", line)

    ask_for_blessing(test)

def check_bad_test(test):
    """Asks the user to bless a test that gave a new output."""
    msg = "Test '%s' gave output different than expected:"
    print(Ansi.clear_screen())
    print(Ansi.in_color(msg % test.name, Ansi.WHITE))
    print("Lines marked with 'E' are from expected output.")
    print("Lines marked with 'A' are from actual output.")
    print()
    # print a pseudo-diff
    expected = test.expected_output().split("\n")[:-1]
    actual = test.output.split("\n")[:-1]
    if len(expected) < len(actual):
        expected += ["<line missing>"] * (len(actual) - len(expected))
    elif len(actual) < len(expected):
        actual += ["<line missing>"] * (len(expected) - len(actual))

    for expected_line, actual_line in zip(expected, actual):
        if expected_line == actual_line:
            print("  %s" % expected_line)
        else:
            print(Ansi.in_color("E %s" % expected_line, Ansi.GREEN))
            print(Ansi.in_color("A %s" % actual_line, Ansi.RED))

    # ask for blessing
    ask_for_blessing(test)

def run_tests(tests, distribution_root):
    print(Ansi.in_color("Running %d tests:" % len(tests), Ansi.WHITE))

    max_width = max(len(test.name) for test in tests)
    test_results = {}
    for test in tests:
        # execute and print result
        print("  %s" % test.name, end="")
        print("." * (max_width + 2 - len(test.name)), end="")
        result = test.run(distribution_root)
        print(" %s" % result)

    # bad tests
    for test in filter(lambda t: t.result == WRONG_OUTPUT, tests):
        check_bad_test(test)

    # first runs
    first_runs = [t for t in tests if t.result == FIRST_RUN]
    for test in first_runs:
        check_new_test(test)

    # final result
    failed_tests = [t for t in tests if t.failed]
    if failed_tests:
        fail_str = "\n%d of %d tests failed." % (len(failed_tests), len(tests))
        print(Ansi.in_color(fail_str, Ansi.LIGHT_RED))
        for test in failed_tests:
            print("  %s" % test.name, end="")
            print("." * (max_width + 2 - len(test.name)), end="")
            print(" %s" % test.result)
        return 1
    else:
        print(Ansi.in_color("\nAll tests passed.", Ansi.WHITE))
        return 0

##############################################
# Running from the command line
##############################################

if __name__ == "__main__":
    import optparse

    # set up option parser
    p = optparse.OptionParser("usage: septest.py [options] <test directories "
                              "or files>")
    p.add_option("-a", "--ansi",
                     action="store_true", dest = "ansi", default=True,
                     help="Enables color output using ANSI sequences.")
    p.add_option("-n", "--no-ansi",
                 action="store_false", dest = "ansi",
                 help="Disables color output using ANSI sequences.")
    options, args = p.parse_args(sys.argv)

    # configure
    if len(args) < 2:
        p.print_usage()
        sys.exit(2)
    Ansi.enabled = options.ansi

    # determine the root of the September distribution
    path_to_septests = path.dirname(args[0])
    distribution_root = path.abspath(path.join(path_to_septests, "../"))

    # run selected tests
    tests = list(sorted(gather_test_files(args[1:]), key=lambda t: t.name))
    clean_leftovers(tests)
    result = run_tests(tests, distribution_root)
    sys.exit(result)
