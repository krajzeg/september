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
        compilation_error = subprocess.call(["python", compiler,
                                             self.source_name,
                                             self.binary_name])
        if compilation_error:
            return COMPILATION_FAILED

        # execute the script
        try:
            interpreter = path.join(distribution_root, "c/09.exe")
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
# Phases of execution
##############################################

def parse_arguments(args):
    """Parses the command line and returns the root directory with the test
    files."""
    if len(args) < 2:
        sys.stderr.write("Usage:\n\tseptests.py <test directory or test "
                         "file>\n")
        sys.exit(2)

    root_dir = args[1]
    if not path.exists(root_dir) or not path.isdir(root_dir):
        sys.stderr.write("Usage:\n\tseptests.py <test directory>\n")
        sys.exit(2)

    return root_dir


def gather_test_files(root_directory):
    """Gather all the test files to be run based on the root directory passed
     by the user."""
    for current_dir, _, files in os.walk(root_directory):
        for file in filter(lambda f: f.endswith(".sep"), files):
            yield TestFile(root_directory, path.join(current_dir, file))


def clean_leftovers(tests):
    """Cleans left-over files from previous executions."""
    for test in tests:
        test.clean()


def ask_for_blessing(test):
    """Asks the user to bless the output of the test, and blesses it if the
    user chooses so.
    """
    print("Is this output correct ([Y]es/[N]o/[S]top)? ", end="")
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
    print("Test '%s' was run for the first time with the following output:" %
          test.name)
    print(test.output)

    ask_for_blessing(test)

def check_bad_test(test):
    """Asks the user to bless a test that gave a new output."""
    print("Test '%s' gave output different than expected:" %
          test.name)
    print("Lines marked with 'E' are from expected output.")
    print("Lines marked with 'A' are from actual output.")

    # print a pseudo-diff
    expected = test.expected_output().split("\n")
    actual = test.output.split("\n")
    if len(expected) < len(actual):
        expected += ["<line missing>"] * (len(actual) - len(expected))
    elif len(actual) < len(expected):
        actual += ["<line missing>"] * (len(expected) - len(actual))

    for expected_line, actual_line in zip(expected, actual):
        if expected_line == actual_line:
            print("  ", expected_line)
        else:
            print("E ", expected_line)
            print("A ", actual_line)

    # ask for blessing
    ask_for_blessing(test)

def run_tests(tests, distribution_root):
    print("Running %d tests:" % len(tests))

    max_width = max(len(test.name) for test in tests)
    test_results = {}
    for test in tests:
        # execute and print result
        print("  %s" % test.name, end="")
        print("." * (max_width + 2 - len(test.name)), end="")
        result = test.run(distribution_root)
        print(" %s" % result)

    # first runs
    first_runs = [t for t in tests if t.result == FIRST_RUN]
    for test in first_runs:
        check_new_test(test)

    # bad tests
    for test in filter(lambda t: t.result == WRONG_OUTPUT, tests):
        check_bad_test(test)

    # final result
    failed_tests = [t for t in tests if t.failed]
    if failed_tests:
        print("\n%d of %d tests failed." % (len(failed_tests), len(tests)))
        for test in failed_tests:
            print("  %s" % test.name, end="")
            print("." * (max_width + 2 - len(test.name)), end="")
            print(" %s" % test.result)
        return 1
    else:
        print("\nAll tests green.")
        return 0

##############################################
# Running from the command line
##############################################

if __name__ == "__main__":
    # find the "09" executable in the 'c' subdirectory
    path_to_septests = path.dirname(sys.argv[0])
    distribution_root = path.abspath(path.join(path_to_septests, "../"))

    # run tests from a chosen directory
    root_dir = parse_arguments(sys.argv)
    tests = list(gather_test_files(root_dir))
    clean_leftovers(tests)
    result = run_tests(tests, distribution_root)
    sys.exit(result)
