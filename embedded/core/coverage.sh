#!/usr/bin/env bash
# Build the core with clang source-based coverage, run the tests, and print a line-coverage report.
#   embedded/core/coverage.sh [build-dir]     (default: build/core-coverage, which isn't committed)
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
build="${1:-$here/../../build/core-coverage}"
cmake -S "$here" -B "$build" -DKATOONI_COVERAGE=ON -DCMAKE_C_COMPILER=clang > /dev/null
cmake --build "$build" > /dev/null
rm -rf "$build/prof"
ctest --test-dir "$build" --output-on-failure
xcrun llvm-profdata merge -sparse "$build"/prof/*.profraw -o "$build/core.profdata"
objs=()
for t in "$build"/test_*; do objs+=(-object "$t"); done
# The first -object flag is dropped: llvm-cov takes the first binary as a positional argument.
xcrun llvm-cov report "${objs[@]:1}" -instr-profile "$build/core.profdata" "$here/src"
