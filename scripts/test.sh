#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

LINT=0
COVERAGE=0

WERROR="${WERROR:-0}"
VERBOSE=""
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"

while [ $# -gt 0 ]; do
    case "$1" in
        -v|--verbose) VERBOSE="-v"; shift ;;
        --lint)       LINT=1; shift ;;
        --coverage)   COVERAGE=1; shift ;;
        --werror)     WERROR=1; shift ;;
        -j)           JOBS="$2"; shift 2 ;;
        -j[0-9]*)     JOBS="${1#-j}"; shift ;;
        --jobs)       JOBS="$2"; shift 2 ;;
        --jobs=*)     JOBS="${1#--jobs=}"; shift ;;
        -h|--help)    sed -n '3,11p' "$0"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

log() { printf '\033[1;34m[test]\033[0m %s\n' "$*"; }

MAKE_FLAGS=()
if [ "$WERROR" = 1 ]; then
    MAKE_FLAGS+=("WERROR=1")
    log "warnings are errors for this build"
fi

if [ "$LINT" = 1 ]; then
    log "running the -Werror lint pass"
    make -j"$JOBS" "${MAKE_FLAGS[@]}" lint
fi

if [ "$COVERAGE" = 1 ]; then
    log "coverage build"
    make -j"$JOBS" clean >/dev/null
    make -j"$JOBS" "${MAKE_FLAGS[@]}" coverage
    if command -v lcov >/dev/null 2>&1; then
        lcov --capture --directory build --output-file build/coverage.info \
             --rc lcov_branch_coverage=1 2>/dev/null || true
        lcov --summary build/coverage.info 2>&1 | sed 's/^/[test]   /' || true
    fi
    exit 0
fi

log "building and running the suite"
make -j"$JOBS" "${MAKE_FLAGS[@]}" test TEST_ARGS="$VERBOSE"

if [ -x ./tests/rvm_tests ]; then
    ./tests/rvm_tests >/dev/null
    log "all suites passed"
fi
