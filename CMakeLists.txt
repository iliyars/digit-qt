cmake_minimum_required(VERSION 3.21)
project(DigitQt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(DIGITQT_BUILD_TESTS "Build unit tests" OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools DataVisualization)

# Needed by BinaryThinningTracker (Fringe Binary Method: adaptive
# threshold + Zhang-Suen skeletonization). On MSYS2 UCRT64:
#   pacman -S mingw-w64-ucrt-x86_64-opencv
find_package(OpenCV REQUIRED COMPONENTS core imgproc)

# Needed by PhaseReconstructor (S2: Laplace/Poisson solve from numbered
# fringe contours to a dense phase map). Header-only. On MSYS2 UCRT64:
#   pacman -S mingw-w64-ucrt-x86_64-eigen3
find_package(Eigen3 REQUIRED)

add_subdirectory(src)
add_subdirectory(translations)

if(DIGITQT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
