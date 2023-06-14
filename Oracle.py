#!/usr/bin/env python3

import sys
import os
import subprocess as sp
import argparse
from oracle_utils import *

parser = argparse.ArgumentParser(description='')
parser.add_argument('--opt', dest='opt', action='store', default="2")
parser.add_argument('--search', dest='needle', action='store', default=None)
parser.add_argument('--sanitizer', dest='sanitizer', action='store', default="address")
parser.add_argument('--fitness', dest='fitness', action='store_true', default=False)
args = parser.parse_args(sys.argv[2:-1])

# The optimization level to use.
opt_level = "-O" + args.opt
# The search needle to look for on O0 (or None)
needle = args.needle
sanitizer = args.sanitizer
# Whether to use the fitness score.
use_scoring = args.fitness or not (needle is None)

compiler = sys.argv[1]
source_file = sys.argv[-1]

def score(msg, score):
    actual_score = score if use_scoring else 0
    giveScore(msg, actual_score)


# Ignore programs that are too large. The fuzzer rarely makes programs
# that are this large, but if they are then avoid that we take down
# the host system by consuming too much memory.
if os.path.getsize(source_file) > 10000:
    score("too large source", -30000000)

# A set of flags passed to all instances.
base_flags = ["-g", "-w"]

is_clang = ("clang++" in compiler)
is_gcc = not is_clang

# The list of sanitizers we want to test.
# TODO: You can shift the order around to avoid and might get different
# results. Note that you can't just shuffle around this list via e.g.
# Python's random module as otherwise the non-deterministic oracle
# confuses the fuzzer.
sanitizers = ["address", "undefined"]
if is_clang:
    sanitizers += ["memory"]

if not sanitizer in sanitizers:
    score("Unknown sanitizer: " + sanitizer, -1000)
sanitizers.remove(sanitizer)
sanitizers = [sanitizer] + sanitizers

# The timeout we use for compiling/running.
timeout = 1

# Keep track if we encountered a sanitizer failure on O0.
had_error = False

# If the user set ASAN_OPTIONS to disable features, re-enable them to
# make sure we don't miss bugs.
os.environ["ASAN_OPTIONS"] = "detect_leaks=1,detect_stack_use_after_return=1"

# Additional string that is sent back to the fuzzer (where it is displayed
# to the user).
extra_info = ""

# Returns true if the given stderr indicates an actual sanitizer failure.
# This is necessary as sanitizers hook into some handlers (e.g., SIGSEGV)
# and pretend they found some kind of UB. But they actually just caught
# some random unrelated crash by accident.
def isSanitizerError(stderr):
    # Stack overflows just disappear on optimization and are always
    # false positives.
    if ': stack-overflow ' in stderr:
        score(prefix + "Ignoring stack-verflow: " + stderr, -80)

    # We hit an error where the sanitizer complains an allocation is too large.
    if 'maximum supported size' in stderr:
        score(prefix + "Ignoring too large allocation err: " + stderr, -80)

    # GCC's UBSan doesn't print anything on segfaults.
    if is_gcc and len(stderr) == 0:
        return False
    
    # Internal GCC error for invalid addresses. Not really a sanitizer check.
    if 'asan/asan_descriptions.cpp' in stderr:
        return False

    # Sanitizers intercepted a random crash and pretend they detected an issue.
    # Ignore this as this is just a false-positive.
    if 'unknown-crash on address' in stderr:
        return False

    # Same as above.
    if 'SEGV on unknown address' in stderr:
        return False

    # Same as above.
    if 'DEADLYSIGNAL' in stderr:
        return False
    return True

# For GCC, we first have to find out if there is an uninitialized use.
# GCC has no memory sanitizer, so an uninitialized use renders the program
# useless for our testing purposes.
if is_gcc:
    binary = compile(compiler, source_file, base_flags)

    try:
        res = sp.run(["valgrind", "--error-exitcode=1", binary],
                     check=True, timeout=5, capture_output=True)
    except sp.TimeoutExpired as e:
        score("Timed out under valgrind", -80)
    except sp.CalledProcessError as e:
        if "uninitialised" in e.stderr.decode("utf-8"):
            # GCC has no MSan, so skip if we find an uninitialized use.
            score("Program depends on uninitialized value.", -80)
        # Otherwise we can search for sanitizer-eliding optimizations.
        pass

# Try compiling with every supported sanitizer and see if we can optimize
# away a sanitizer error. Note that we can't just enable all of them
# at once as this just not compiles at all or causes bogus issues.
for sanitizer in sanitizers:
    print("Testing sanitizer " + sanitizer)

    flags = base_flags + ["-fsanitize=" + sanitizer]

    sys.stdout.write("  -O0: ")
    prefix = "[" + sanitizer + "] "
    # First try without optimization.
    try:
        # First make sure the program has an sanitizer on O0.
        compileAndRun(compiler, source_file, flags)
        print(" No error on -O0")
    except FailedToCompile as e:
        # Just ignore programs if they somehow fail to compile.
        score(prefix + "Test program failed to compile: " + e.stderr.decode("utf-8"), -80)
    except TimeOutRunning as e:
        # If we timed out compiling then ignore the program.
        score(prefix + "Test program timed out", -80)
    except FailedToRun as e:
        stderr = e.stderr.decode("utf-8")
        # If we have a needle to look for on O0, check first and then abort if
        # it's not there.
        if needle and not (needle in stderr):
            score("Can't find search string in output", -1)
        # On error, filter out false positives.
        if isSanitizerError(e.stderr.decode("utf-8")):
            had_error = True
            print(" Detected error")
        else:
            print(" No error")

    # Try running with optimizations.
    sys.stdout.write("  " + opt_level + ": ")
    try:
        compileAndRun(compiler, source_file, flags + [opt_level])
        print(" No error on " + opt_level)
    except FailedToCompile as e:
        # This really should never happen, but e.g., ICE's can cause this.
        score(prefix + "Optimized program failed to compile???", -80)
    except TimeOutRunning as e:
        # Ignore timeouts which are usually non-deterministic.
        score(prefix + "Failed to compile optimized program", -80)
    except FailedToRun as e:
        internal_crash = False
        # There is an optional check in libc that reports double free's.
        # This only happens when the sanitizer failed to detect the
        # double free itself.
        if "free(): double free detected in tcache 2" in e.stderr.decode("utf-8"):
            internal_crash = True
            extra_info = "(Bypassed sanitizer and crashed in libc)"

        # If we still find the issue after optimizations we didn't find an SEO.
        # Abort to save time. This can't be an SEO.
        if not internal_crash:
            print("stderr:" + e.stderr.decode("utf-8"))
            score(prefix + "Failure still found after optimization", -80)

# If we didn't find any errors on O0 then we failed to make a buggy program.
if not had_error:
    score("Program had no sanitizer error on O0", 0)

# We had an error on O0 and no iteration of the loop above never aborted
# this script, so there is no error on any O2 binary.
markInteresting("Error is gone " + extra_info)
