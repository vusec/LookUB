# LookUB

LookUB is an automatic detection system for optimizations that elide sanitizer
checks.

## Quick start

```bash
# Clone repository.
$ git clone https://github.com/<anonymous>/LookUB
# Create a build directory.
$ mkdir -p LookUB/build ; cd LookUB/build
# Build
$ cmake .. && make
# Search for eliding optimizations.
$ ./bin/LookUB -- ../Oracle.py /usr/bin/clang++
```

The generated test cases demonstrating sanitizer-eliding optimizations (SEO)
will be stored in the `saved_testcases` directory.

## Command line arguments

```bash
LookUB [MutatorArguments] -- ./Oracle.py /usr/bin/clang++ [OracleArguments]
```

### Mutator arguments.

These go before the `--` in the command line above.

* `--simple-ui`: Disables the interactive UI.
* `--stop-after-hit`: Stop fuzzing after finding a SEO (default: disabled).
* `--stop-after=N`: Stop fuzzing after `N` SEOs (default: never).
* `--scale=N`: How many mutations to apply each iteration (default: 1).

* `--tries=N`: How many tries to give each program.
* `--queue-size=N`: How many programs to keep in the queue.
* `--step`: Manual stepping mode. Press space to generate and run a program.
* `--reducer-tries=N`: How many tries to reduce programs.
* `--ui-update=N`: UI update frequency (in ms)
* `--splash`: Whether to show a startup splash.

### Oracle arguments.

* `--opt=N`: Changes the optimization level to `N`. (default: `2` for `-O2`).
* `--fitness`: Whether to use the fitness function (default: disabled).
* `--search=STRING`: The search string to look for on O0. This is useful if you
want to look for a specific sanitizer error. (default: None)
* `--sanitizer=STRING`: The sanitizer to run first and search the `--search`
string with. One of `address`, `undefined` or `memory`. (default: `address`)