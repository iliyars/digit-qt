# Defines the single DigitQt executable target. Every subdirectory below
# (core/, gui/, ...) adds its own files to this same target via
# target_sources(DigitQt PRIVATE ...) -- mirrors the layout used by Flameshot
# (src/cli, src/config, src/core, ...). ApertureCore is the one exception:
# it is a genuinely independent library (no Qt dependency, unit-testable on
# its own), so it gets its own CMake target instead.

add_executable(DigitQt WIN32
    main.cpp
)
add_executable(DigitQt::App ALIAS DigitQt)

# Lets every module #include "core/Measurement.h", "gui/MainWindow.h", etc.
# using paths relative to src/.
target_include_directories(DigitQt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(aperture)  # independent library
add_subdirectory(core)          # target_sources(DigitQt ...)
add_subdirectory(io)            # target_sources(DigitQt ...)
add_subdirectory(gui)           # target_sources(DigitQt ...)

target_link_libraries(DigitQt PRIVATE Qt6::Widgets Aperture::Core)

set_target_properties(DigitQt PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
