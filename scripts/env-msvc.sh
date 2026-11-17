#!/usr/bin/env bash
# Exports the environment MSVC needs, without running vcvars64.bat.
#
# Why not vcvars: it derives part of its state through registry queries and
# fails in restricted shells, leaving rc.exe and the SDK paths unset. The
# variables below are reconstructed from the on-disk layout instead, which is
# enough for cl.exe plus link.exe under a Ninja generator.
#
# Usage:  source scripts/env-msvc.sh
# Override VS_ROOT / SDK_ROOT / MSVC_VER / SDK_VER to pin a different toolchain.

set -u

VS_ROOT="${VS_ROOT:-/c/Program Files/Microsoft Visual Studio/2022/Community}"
SDK_ROOT="${SDK_ROOT:-/c/Program Files (x86)/Windows Kits/10}"

pick_latest() {
    local dir="$1"
    [ -d "$dir" ] || return 1
    ls "$dir" 2>/dev/null | sort -V | tail -1
}

MSVC_VER="${MSVC_VER:-$(pick_latest "$VS_ROOT/VC/Tools/MSVC")}"
SDK_VER="${SDK_VER:-$(pick_latest "$SDK_ROOT/Include")}"

if [ -z "${MSVC_VER:-}" ] || [ -z "${SDK_VER:-}" ]; then
    echo "env-msvc: cannot locate MSVC or Windows SDK under $VS_ROOT / $SDK_ROOT" >&2
    return 1 2>/dev/null || exit 1
fi

VC="$VS_ROOT/VC/Tools/MSVC/$MSVC_VER"
SDK_INC="$SDK_ROOT/Include/$SDK_VER"
SDK_LIB="$SDK_ROOT/Lib/$SDK_VER"
VC_BIN="$VC/bin/Hostx64/x64"
SDK_BIN="$SDK_ROOT/bin/$SDK_VER/x64"

if [ ! -x "$VC_BIN/cl.exe" ]; then
    echo "env-msvc: cl.exe not found at $VC_BIN" >&2
    return 1 2>/dev/null || exit 1
fi

win() { cygpath -w "$1"; }

export INCLUDE="$(win "$VC/include");$(win "$SDK_INC/ucrt");$(win "$SDK_INC/um");$(win "$SDK_INC/shared")"
export LIB="$(win "$VC/lib/x64");$(win "$SDK_LIB/ucrt/x64");$(win "$SDK_LIB/um/x64")"
export PATH="$VC_BIN:$SDK_BIN:$PATH"
export VSLANG=1033

echo "env-msvc: MSVC $MSVC_VER, Windows SDK $SDK_VER"
