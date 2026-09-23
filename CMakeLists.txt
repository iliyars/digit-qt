cmake_minimum_required(VERSION 3.21)
project(DigitQt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(DIGITQT_BUILD_TESTS "Build unit tests" OFF)
option(DIGITQT_BUILD_TOOLS "Build diagnostic CLI tools (see tools/)" OFF)
option(DIGITQT_PROBE_CORE_QT_FREE "Discovery build: compile src/core/ as a standalone library with no Qt include paths/linkage, to enumerate every Qt dependency as a compiler error (see notes/ core/Qt decoupling discussion). Not part of the shipped app." OFF)

find_package(Qt5 REQUIRED COMPONENTS Widgets LinguistTools DataVisualization Concurrent)

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

if(DIGITQT_PROBE_CORE_QT_FREE)
    # Compiles every src/core/*.cpp a second time into an isolated target
    # that never sees Qt's include paths or libraries. core/ is still built
    # into DigitQt as usual via target_sources() elsewhere -- this target
    # changes nothing about the shipped app. Its only purpose is that any
    # #include <Q...> (direct or transitive) in core/ now fails as a
    # compiler error naming the exact file, instead of silently compiling
    # because Qt happens to be on DigitQt's include path.
    file(GLOB_RECURSE DIGITQT_CORE_PROBE_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/core/*.cpp")
    add_library(digitqt_core_qt_free_probe STATIC EXCLUDE_FROM_ALL
        ${DIGITQT_CORE_PROBE_SOURCES})
    target_include_directories(digitqt_core_qt_free_probe PRIVATE
        "${CMAKE_SOURCE_DIR}/src" ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(digitqt_core_qt_free_probe PRIVATE
        Aperture::Core ${OpenCV_LIBS} Eigen3::Eigen)
    target_compile_features(digitqt_core_qt_free_probe PRIVATE cxx_std_17)
endif()

if(DIGITQT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(DIGITQT_BUILD_TOOLS)
    add_subdirectory(tools)
endif()
