#!/usr/bin/env bash
# Configures, builds and tests the core library, prepare tool and unit tests.
# Windows / MSVC only for now; other platforms will get their own script rather
# than conditionals here.

set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-$root/build}"
build_type="${BUILD_TYPE:-RelWithDebInfo}"

# shellcheck source=/dev/null
source "$root/scripts/env-msvc.sh"

# Ninja is not a Windows component; pick it up from the Python environment that
# installed it if it is missing from PATH. Python reports a Windows path, so it
# must be converted before bash can test it.
ninja_program="$(command -v ninja 2>/dev/null || true)"
if [ -z "$ninja_program" ]; then
    py_scripts="$(python -c 'import sysconfig; print(sysconfig.get_path("scripts"))' 2>/dev/null || true)"
    if [ -n "$py_scripts" ]; then
        py_scripts="$(cygpath -u "$py_scripts")"
    fi
    if [ -n "$py_scripts" ] && [ -x "$py_scripts/ninja.exe" ]; then
        export PATH="$py_scripts:$PATH"
        ninja_program="$py_scripts/ninja.exe"
    else
        echo "build: ninja not found. Install it with: python -m pip install ninja" >&2
        exit 1
    fi
fi

cmake -S "$root" -B "$build_dir" -G Ninja \
    -DCMAKE_MAKE_PROGRAM="$ninja_program" \
    -DCMAKE_BUILD_TYPE="$build_type"
cmake --build "$build_dir"
ctest --test-dir "$build_dir" --output-on-failure
