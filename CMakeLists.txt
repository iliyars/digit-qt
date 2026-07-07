cmake_minimum_required(VERSION 3.21)
project(DigitQt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(DIGITQT_BUILD_TESTS "Build unit tests" OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools)

add_subdirectory(src)
add_subdirectory(translations)

if(DIGITQT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
