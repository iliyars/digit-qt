cmake_minimum_required(VERSION 3.21)
project(DigitQt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(DIGITQT_BUILD_TESTS "Build unit tests" OFF)

find_package(Qt5 REQUIRED COMPONENTS Widgets LinguistTools DataVisualization)

# Needed by BinaryThinningTracker (Fringe Binary Method: adaptive
# threshold + Zhang-Suen skeletonization). On MSYS2 MINGW64:
#   pacman -S mingw-w64-x86_64-opencv
find_package(OpenCV REQUIRED COMPONENTS core imgproc)

# Needed by ModalAnalysisStage (Zernike/Gram-Schmidt fit over the
# aperture). Header-only. On MSYS2 MINGW64:
#   pacman -S mingw-w64-x86_64-eigen3
find_package(Eigen3 REQUIRED)

if(WIN32)
    # Target Windows 7 as the minimum supported OS. Qt6 dropped Windows 7
    # support outright, which is why this project uses Qt5 (5.15 LTS, the
    # last release with official Win7 support) on MSYS2's MINGW64
    # environment (msvcrt-based -- unlike UCRT64, it needs no separate
    # "Universal C Runtime" update on Windows 7/8.1).
    add_compile_definitions(_WIN32_WINNT=0x0601 WINVER=0x0601)
endif()

add_subdirectory(src)
add_subdirectory(translations)

if(DIGITQT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
