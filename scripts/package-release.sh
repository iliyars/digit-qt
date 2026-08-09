#!/usr/bin/env bash
# Собирает автономный Release-дистрибутив DigitQt для Windows (x64,
# mingw64/Qt5) в dist/ и упаковывает его в zip -- то же самое, что делает
# .github/workflows/release.yaml, но локально, чтобы проверить упаковку
# до пуша тега. Запускать через настоящий MSYS2 bash (C:\msys64\usr\bin\
# bash.exe -- например `bash scripts/package-release.sh` из PowerShell),
# не через Git Bash: скрипт полагается на собственную точку монтирования
# MSYS2 /mingw64, а не на одноимённую (но другую) в Git Bash.
#
# Использование: scripts/package-release.sh [версия]
#   версия по умолчанию берётся из `git describe --tags --always --dirty`.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."
REPO_ROOT="$(pwd)"

# /mingw64 is MSYS2's own stable mount point for the MINGW64 environment
# -- it resolves correctly regardless of which drive/directory MSYS2 was
# actually installed to (hardcoding /c/msys64/mingw64 broke on the
# GitHub-hosted runner, which installs it elsewhere).
MINGW64=/mingw64
BUILD_DIR=build-release
DIST_DIR=dist
# Static, TBB/IPP-free OpenCV (core+imgproc only) built by
# build-opencv-win7.sh into a sibling directory of the repo (see that
# script for why -- same convention here so this resolves identically
# on any machine, including CI runners). The system
# mingw-w64-x86_64-opencv package pulls in libtbb12.dll, which imports
# GetCurrentThreadStackLimits -- a Windows 8+ kernel32 API that doesn't
# exist on Windows 7, so a release linked against it fails to even
# start there ("entry point not found"). The top-level
# OpenCVConfig.cmake is a dispatcher for the official prebuilt Windows
# package layout and doesn't recognize our custom --install; point
# straight at the arch/runtime/linkage-specific config it generates
# underneath.
OPENCV_WIN7_DIR="$REPO_ROOT/../opencv-win7-mingw64/install/x64/mingw/staticlib"

# Depending on how bash was launched (MSYS2 terminal vs. plain bash.exe
# from PowerShell/cmd), Windows' own System32 can shadow MSYS core
# utils on PATH -- notably `sort.exe`, which doesn't understand `-u`
# and fails with a cryptic "-u: file not found". Force MSYS/mingw64
# first.
export PATH="/usr/bin:$MINGW64/bin:$PATH"

VERSION="${1:-$(git describe --tags --always --dirty)}"
ZIP_NAME="DigitQt-${VERSION}-windows-x64.zip"

require() {
  if [ ! -x "$1" ]; then
    echo "Missing required tool: $1" >&2
    echo "Install with: pacman -S --needed mingw-w64-x86_64-ntldd mingw-w64-x86_64-7zip" >&2
    exit 1
  fi
}
require "$MINGW64/bin/g++.exe"
require "$MINGW64/bin/ninja.exe"
require "$MINGW64/bin/windeployqt-qt5.exe"
require "$MINGW64/bin/ntldd.exe"
require "$MINGW64/bin/7z.exe"

if [ ! -f "$OPENCV_WIN7_DIR/OpenCVConfig.cmake" ]; then
  echo "Missing Win7-safe OpenCV build: $OPENCV_WIN7_DIR" >&2
  echo "Build it first with: scripts/build-opencv-win7.sh" >&2
  exit 1
fi

echo "==> Configuring Release build ($BUILD_DIR, version $VERSION)"
cmake -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$MINGW64" \
  -DCMAKE_CXX_COMPILER="$MINGW64/bin/g++.exe" \
  -DOpenCV_DIR="$OPENCV_WIN7_DIR"

echo "==> Building"
cmake --build "$BUILD_DIR" --parallel

echo "==> Preparing $DIST_DIR"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"
cp "$BUILD_DIR/src/DigitQt.exe" "$DIST_DIR/"
cp -r "$BUILD_DIR/src/translations" "$DIST_DIR/"

echo "==> Deploying Qt dependencies"
"$MINGW64/bin/windeployqt-qt5.exe" "$DIST_DIR/DigitQt.exe" --release --no-translations --compiler-runtime --no-angle --no-opengl-sw

echo "==> Deploying OpenCV & MinGW runtime"
# ntldd prints resolved paths with backslashes and the real Windows
# drive/directory MSYS2 lives in (e.g.
# "D:\msys64\mingw64\bin\libgcc_s_seh-1.dll => ..."), which we don't
# know in advance -- ask cygpath instead of hardcoding it. Normalize
# both sides to forward slashes so the comparison doesn't have to deal
# with backslash regex-escaping. `|| true` on the grep: no matches
# would otherwise make the pipeline fail under `pipefail` and abort the
# script.
MINGW64_WIN_FWD="$(cygpath -w "$MINGW64" | tr '\\' '/')"
"$MINGW64/bin/ntldd.exe" -R "$DIST_DIR/DigitQt.exe" \
  | tr '\\' '/' \
  | { grep -oi "$MINGW64_WIN_FWD/bin/[^ ]*\.dll" || true; } \
  | while read -r dll; do
      cp "$dll" "$DIST_DIR/" 2>/dev/null || true
    done

echo "==> Packaging $ZIP_NAME"
rm -f "$ZIP_NAME"
(cd "$DIST_DIR" && "$MINGW64/bin/7z.exe" a "../$ZIP_NAME" .)

echo "==> Done: $ZIP_NAME ($(du -h "$ZIP_NAME" | cut -f1))"
echo "==> Test it: unzip on a clean machine (or a VM) and run DigitQt.exe -- it must not need Qt/MSYS2/VC++ redist installed."
