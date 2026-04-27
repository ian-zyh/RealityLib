#!/bin/bash
# Setup script for RealityLib dependencies on Mac
# Run this script from the project root directory

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DEPS_DIR="$PROJECT_ROOT/app/src/main/deps"

echo "=== RealityLib Dependency Setup ==="
echo "Project root: $PROJECT_ROOT"
echo "Dependencies directory: $DEPS_DIR"

# Create deps directories if they don't exist
mkdir -p "$DEPS_DIR/OpenXR-SDK/include"
mkdir -p "$DEPS_DIR/OpenXR-SDK/libs/arm64-v8a"

# Download OpenXR SDK headers from Khronos
echo ""
echo "=== Step 1: Downloading OpenXR SDK headers ==="
OPENXR_VERSION="1.1.38"
OPENXR_DOWNLOAD_URL="https://github.com/KhronosGroup/OpenXR-SDK/archive/refs/tags/release-${OPENXR_VERSION}.zip"
TMP_DIR="$(mktemp -d 2>/dev/null || mktemp -d -t realitylib-deps)"
ZIP_PATH="${TMP_DIR}/OpenXR-SDK-release-${OPENXR_VERSION}.zip"
EXTRACT_DIR="${TMP_DIR}/OpenXR-SDK"

cleanup() {
    rm -rf "${TMP_DIR}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

echo "Downloading OpenXR SDK ${OPENXR_VERSION}..."
if ! curl -fL --retry 3 --retry-delay 2 -o "${ZIP_PATH}" "${OPENXR_DOWNLOAD_URL}"; then
    echo "✗ Failed to download OpenXR headers"
    echo "  URL: ${OPENXR_DOWNLOAD_URL}"
    exit 1
fi

# Validate that what we downloaded is actually a zip (GitHub can return HTML on 404/blocked network/etc.)
if ! unzip -tq "${ZIP_PATH}" >/dev/null 2>&1; then
    echo "✗ Downloaded file is not a valid zip archive"
    echo "  URL: ${OPENXR_DOWNLOAD_URL}"
    echo "  Saved as: ${ZIP_PATH}"
    echo "  Tip: If you're behind a proxy/captive portal, the download may be an HTML page."
    exit 1
fi

echo "Extracting OpenXR SDK headers..."
mkdir -p "${EXTRACT_DIR}"
unzip -q -o "${ZIP_PATH}" -d "${EXTRACT_DIR}"

SDK_ROOT_GLOB=("${EXTRACT_DIR}"/OpenXR-SDK-*)
SDK_ROOT="${SDK_ROOT_GLOB[0]}"
if [ ! -d "${SDK_ROOT}" ]; then
    echo "✗ Could not find extracted OpenXR-SDK directory"
    exit 1
fi

if [ ! -f "${SDK_ROOT}/include/openxr/openxr.h" ]; then
    echo "✗ Extracted OpenXR headers not found at expected path:"
    echo "  ${SDK_ROOT}/include/openxr/openxr.h"
    exit 1
fi

rm -rf "$DEPS_DIR/OpenXR-SDK/include/openxr"
cp -R "${SDK_ROOT}/include/openxr" "$DEPS_DIR/OpenXR-SDK/include/"
echo "✓ OpenXR headers installed"

# Check for OpenXR loader
echo ""
echo "=== Step 2: Checking OpenXR Loader ==="
LOADER_PATH="$DEPS_DIR/OpenXR-SDK/libs/arm64-v8a/libopenxr_loader.so"

if [ -f "$LOADER_PATH" ]; then
    echo "✓ OpenXR loader already present"
else
    echo "✗ OpenXR loader NOT found"
    echo ""
    echo "╔══════════════════════════════════════════════════════════════════╗"
    echo "║                    MANUAL STEP REQUIRED                          ║"
    echo "╠══════════════════════════════════════════════════════════════════╣"
    echo "║ The Meta OpenXR loader library is required to build for Quest.   ║"
    echo "║                                                                  ║"
    echo "║ OPTION 1: Download from Meta Developer Portal                    ║"
    echo "║   1. Visit: https://developer.oculus.com/downloads/              ║"
    echo "║   2. Search for 'OpenXR Mobile SDK'                              ║"
    echo "║   3. Download and extract the zip file                           ║"
    echo "║   4. Copy the file:                                              ║"
    echo "║      OpenXR/Libs/Android/arm64-v8a/Release/libopenxr_loader.so   ║"
    echo "║      to: deps/OpenXR-SDK/libs/arm64-v8a/                         ║"
    echo "║                                                                  ║"
    echo "║ OPTION 2: Use Meta XR SDK Unity/Unreal                           ║"
    echo "║   If you have Unity or Unreal with Meta XR SDK installed,        ║"
    echo "║   the loader can be found in the plugin directories.             ║"
    echo "║                                                                  ║"
    echo "║ OPTION 3: Build from Meta's GitHub                               ║"
    echo "║   Clone: https://github.com/meta-xr/openxr-loader-android        ║"
    echo "║   Follow build instructions to generate the .so file.            ║"
    echo "╚══════════════════════════════════════════════════════════════════╝"
    echo ""
    echo "After obtaining the loader, place it at:"
    echo "  $LOADER_PATH"
fi

# Create local.properties if it doesn't exist
echo ""
echo "=== Step 3: Checking local.properties ==="
if [ ! -f "$PROJECT_ROOT/local.properties" ]; then
    echo "Creating local.properties..."
    ANDROID_SDK_PATH="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}}"
    echo "sdk.dir=$ANDROID_SDK_PATH" > "$PROJECT_ROOT/local.properties"
    echo "✓ Created local.properties with sdk.dir=$ANDROID_SDK_PATH"
else
    echo "✓ local.properties already exists"
fi

# Check JAVA_HOME
echo ""
echo "=== Step 4: Checking Java ==="
if [ -n "$JAVA_HOME" ] && [ -x "$JAVA_HOME/bin/java" ]; then
    echo "✓ JAVA_HOME is set: $JAVA_HOME"
elif [ -x "/Applications/Android Studio.app/Contents/jbr/Contents/Home/bin/java" ]; then
    echo "✓ Found JDK bundled with Android Studio"
    echo "  Set JAVA_HOME with:"
    echo "  export JAVA_HOME=\"/Applications/Android Studio.app/Contents/jbr/Contents/Home\""
else
    echo "✗ Java not found. Install JDK 11+ or Android Studio."
fi

# Summary
echo ""
echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║                      SETUP SUMMARY                               ║"
echo "╠══════════════════════════════════════════════════════════════════╣"
if [ -d "$DEPS_DIR/OpenXR-SDK/include/openxr" ]; then
    echo "║  ✓ OpenXR headers     - Installed                                ║"
else
    echo "║  ✗ OpenXR headers     - Missing (run script again)               ║"
fi
if [ -f "$LOADER_PATH" ]; then
    echo "║  ✓ OpenXR loader      - Ready                                    ║"
else
    echo "║  ✗ OpenXR loader      - MISSING (see instructions above)         ║"
fi
if [ -f "$PROJECT_ROOT/local.properties" ]; then
    echo "║  ✓ local.properties   - Present                                  ║"
else
    echo "║  ✗ local.properties   - Missing                                  ║"
fi
echo "╚══════════════════════════════════════════════════════════════════╝"

echo ""
echo "=== Build Commands ==="
echo ""
echo "Once the OpenXR loader is in place, build with:"
echo ""
echo "  export JAVA_HOME=\"/Applications/Android Studio.app/Contents/jbr/Contents/Home\""
echo "  ./gradlew assembleDebug"
echo ""
echo "Install on Quest:"
echo ""
echo "  adb install -r app/build/outputs/apk/debug/app-debug.apk"
echo ""
