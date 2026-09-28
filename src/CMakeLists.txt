# Defines the single DigitQt executable target. Most subdirectories below
# (io/, gui/, core/commands/) add their own files to this same target via
# target_sources(DigitQt PRIVATE ...) -- mirrors the layout used by Flameshot
# (src/cli, src/config, src/core, ...). aperture and core are the two
# exceptions: both are genuinely independent libraries (no Qt dependency,
# unit-testable on their own -- see src/core/CMakeLists.txt for why
# core/commands/ stays out of the core library), so they get their own
# CMake targets instead.

add_executable(DigitQt WIN32
    main.cpp
)
add_executable(DigitQt::App ALIAS DigitQt)

# Lets every module #include "core/Measurement.h", "gui/MainWindow.h", etc.
# using paths relative to src/.
target_include_directories(DigitQt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${OpenCV_INCLUDE_DIRS})

add_subdirectory(aperture)      # independent library
add_subdirectory(core)          # independent library (+ commands/: target_sources(DigitQt ...))
add_subdirectory(io)            # target_sources(DigitQt ...)
add_subdirectory(gui)           # target_sources(DigitQt ...)
add_subdirectory(plugin_host)   # target_sources(DigitQt ...) -- loads DqtFringeTracer plugins

target_link_libraries(DigitQt PRIVATE Qt5::Widgets Qt5::DataVisualization Qt5::Concurrent
                                      DigitQt::Core DigitQt::PluginAbi Aperture::Core
                                      ${OpenCV_LIBS} Eigen3::Eigen)

set_target_properties(DigitQt PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
