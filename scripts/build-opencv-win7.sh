#!/usr/bin/env bash
# Собирает урезанный статический OpenCV (только core+imgproc -- всё, что
# использует DigitQt, см. BinaryThinningTracker.cpp) для релизных сборок
# под Windows 7.
#
# Зачем: пакет mingw-w64-x86_64-opencv из MSYS2 тянет за собой TBB
# (libtbb12.dll), который импортирует GetCurrentThreadStackLimits --
# API, появившийся только в Windows 8. Из-за этого DigitQt.exe,
# собранный против системного OpenCV, не запускается на Windows 7
# ("Точка входа не найдена"), даже с Qt5/mingw64 и _WIN32_WINNT=0x0601.
# Эта сборка отключает TBB/IPP и все ненужные модули/кодеки и линкуется
# статически, так что DigitQt.exe вообще не зависит от какой-либо
# opencv-related DLL.
#
# Результат ставится в ../opencv-win7-mingw64/install (рядом с
# репозиторием, не внутри него -- это отдельная толстая зависимость,
# собирается один раз и переиспользуется). scripts/package-release.sh
# подхватывает её через -DOpenCV_DIR.
#
# Использование: scripts/build-opencv-win7.sh
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."
REPO_ROOT="$(pwd)"

# /mingw64 is MSYS2's own stable mount point for the MINGW64 environment
# -- it resolves correctly regardless of which drive/directory MSYS2 was
# actually installed to (hardcoding /c/msys64/mingw64 broke on the
# GitHub-hosted runner, which installs it elsewhere).
MINGW64=/mingw64
export PATH="/usr/bin:$MINGW64/bin:$PATH"

OPENCV_VERSION=4.13.0
WORKDIR="$REPO_ROOT/../opencv-win7-mingw64"
SRC_DIR="$WORKDIR/src"
BUILD_DIR="$WORKDIR/build"
INSTALL_DIR="$WORKDIR/install"

require() {
  if [ ! -x "$1" ]; then
    echo "Missing required tool: $1" >&2
    exit 1
  fi
}
require "$MINGW64/bin/g++.exe"
require "$MINGW64/bin/ninja.exe"

if [ -f "$INSTALL_DIR/x64/mingw/staticlib/OpenCVConfig.cmake" ]; then
  echo "==> $INSTALL_DIR already has a built OpenCV, skipping (delete it to force a rebuild)"
  exit 0
fi

mkdir -p "$WORKDIR"

if [ -d "$SRC_DIR" ]; then
  echo "==> $SRC_DIR already exists, skipping clone"
else
  echo "==> Cloning OpenCV $OPENCV_VERSION"
  git clone --branch "$OPENCV_VERSION" --depth 1 https://github.com/opencv/opencv.git "$SRC_DIR"
fi

echo "==> Configuring (core+imgproc only, static, no TBB/IPP, Win7 target)"
cmake -B "$BUILD_DIR" -S "$SRC_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
  -DCMAKE_CXX_COMPILER="$MINGW64/bin/g++.exe" \
  -DCMAKE_C_COMPILER="$MINGW64/bin/gcc.exe" \
  -DCMAKE_CXX_FLAGS="-D_WIN32_WINNT=0x0601 -DWINVER=0x0601" \
  -DCMAKE_C_FLAGS="-D_WIN32_WINNT=0x0601 -DWINVER=0x0601" \
  -DBUILD_LIST=core,imgproc \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOCS=OFF \
  -DBUILD_opencv_apps=OFF -DBUILD_opencv_python2=OFF -DBUILD_opencv_python3=OFF -DBUILD_opencv_java=OFF \
  -DBUILD_opencv_imgcodecs=OFF -DBUILD_opencv_videoio=OFF -DBUILD_opencv_highgui=OFF \
  -DWITH_TBB=OFF -DWITH_IPP=OFF -DWITH_ITT=OFF -DWITH_OPENMP=OFF \
  -DWITH_OPENCL=OFF -DWITH_CUDA=OFF -DWITH_LAPACK=OFF \
  -DWITH_PNG=OFF -DWITH_JPEG=OFF -DWITH_TIFF=OFF -DWITH_WEBP=OFF -DWITH_OPENJPEG=OFF -DWITH_JASPER=OFF -DWITH_OPENEXR=OFF \
  -DWITH_FFMPEG=OFF -DWITH_GSTREAMER=OFF -DWITH_V4L=OFF -DWITH_DSHOW=OFF -DWITH_MSMF=OFF -DWITH_1394=OFF -DWITH_QUIRC=OFF -DWITH_PROTOBUF=OFF \
  -DWITH_ADE=OFF

echo "==> Building"
cmake --build "$BUILD_DIR" --parallel

echo "==> Installing to $INSTALL_DIR"
cmake --install "$BUILD_DIR"

echo "==> Verifying no TBB/GetCurrentThreadStackLimits leaked in"
if find "$INSTALL_DIR" -iname "*.a" -exec "$MINGW64/bin/objdump.exe" -p {} \; 2>/dev/null | grep -qi "GetCurrentThreadStackLimits"; then
  echo "WARNING: GetCurrentThreadStackLimits still referenced -- Win7 will still fail to load." >&2
  exit 1
fi

echo "==> Done: $INSTALL_DIR"
