#!/usr/bin/env bash
# build-local.sh — Build libVehFuncs64.so locally using the Android NDK.
#
# This is the local equivalent of .github/workflows/build.yml.
# Useful for iterating faster than waiting for CI.
#
# Prereqs:
#   - Android NDK r21+ installed (set $ANDROID_NDK_HOME or $NDK_HOME)
#   - bash, git
#
# Usage:
#   ./scripts/build-local.sh                 # full build
#   ./scripts/build-local.sh --skip-bootstrap # skip the AML header fetch
#   ./scripts/build-local.sh --clean          # ndk-build clean first

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

SKIP_BOOTSTRAP=0
DO_CLEAN=0
for arg in "$@"; do
    case "$arg" in
        --skip-bootstrap) SKIP_BOOTSTRAP=1 ;;
        --clean)          DO_CLEAN=1 ;;
        *) echo "Unknown arg: $arg"; exit 1 ;;
    esac
done

# ---- Locate NDK ----
NDK_DIR="${ANDROID_NDK_HOME:-${NDK_HOME:-}}"
if [[ -z "$NDK_DIR" || ! -x "$NDK_DIR/ndk-build" ]]; then
    # Try common install paths
    for candidate in \
        "$HOME/Android/Sdk/ndk/"* \
        "$HOME/Library/Android/sdk/ndk/"* \
        "/opt/android-ndk" \
        "/usr/local/android-ndk"; do
        if [[ -x "$candidate/ndk-build" ]]; then
            NDK_DIR="$candidate"
            break
        fi
    done
fi
if [[ -z "$NDK_DIR" || ! -x "$NDK_DIR/ndk-build" ]]; then
    echo "ERROR: Android NDK not found." >&2
    echo "Set ANDROID_NDK_HOME to your NDK install path, e.g.:" >&2
    echo "  export ANDROID_NDK_HOME=\$HOME/Android/Sdk/ndk/27.2.12479018" >&2
    exit 1
fi
echo "==> Using NDK at: $NDK_DIR"

# ---- Bootstrap AML headers (unless --skip-bootstrap) ----
if [[ $SKIP_BOOTSTRAP -eq 0 ]]; then
    if [[ ! -f "$PROJECT_DIR/include/mod/amlmod.h" \
          || -d "$PROJECT_DIR/include/mod.placeholder-backup" ]]; then
        # Either no amlmod.h, OR placeholders were never replaced
        echo "==> Bootstrapping AML headers..."
        bash "$SCRIPT_DIR/bootstrap-aml-headers.sh"
    else
        echo "==> AML headers already in place (use bootstrap-aml-headers.sh --force to refresh)"
    fi
fi

# ---- Clean (optional) ----
if [[ $DO_CLEAN -eq 1 ]]; then
    echo "==> Cleaning previous build..."
    rm -rf "$PROJECT_DIR/libs" "$PROJECT_DIR/obj"
fi

# ---- Run ndk-build ----
echo "==> Running ndk-build..."
cd "$PROJECT_DIR"
export PATH="$NDK_DIR:$PATH"
"$NDK_DIR/ndk-build" \
    NDK_PROJECT_PATH="$PROJECT_DIR" \
    APP_BUILD_SCRIPT="$PROJECT_DIR/Android.mk" \
    NDK_APPLICATION_MK="$PROJECT_DIR/Application.mk" \
    APP_ABI=arm64-v8a \
    APP_PLATFORM=android-21 \
    -j4

# ---- Verify output ----
SO="$PROJECT_DIR/libs/arm64-v8a/libVehFuncs64.so"
if [[ ! -f "$SO" ]]; then
    echo "ERROR: build did not produce $SO" >&2
    exit 1
fi

echo
echo "==> Build succeeded!"
echo "    Output: $SO"
echo "    Size:   $(stat -c %s "$SO") bytes"
echo
echo "    Install on device:"
echo "      adb push \"$SO\" /sdcard/Android/data/com.rockstargames.gtasa/mods/"
echo "      adb logcat -s AML VehFuncs"
