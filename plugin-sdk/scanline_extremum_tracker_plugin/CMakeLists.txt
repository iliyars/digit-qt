# ScanlineExtremumTracker за границей DqtFringeTracer C ABI -- см.
# sequential_fringe_tracker_plugin/CMakeLists.txt. Единственный из 4
# трекеров, реально использующий setParam()/order-hasOrder (ABI v2).

add_library(scanline_extremum_tracker_plugin MODULE
    plugin.cpp
)

target_link_libraries(scanline_extremum_tracker_plugin PRIVATE DigitQt::Core DigitQt::PluginAbi)

set_target_properties(scanline_extremum_tracker_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    PREFIX ""
    OUTPUT_NAME "scanline_extremum_tracker"
)

# См. binary_thinning_poc/CMakeLists.txt -- копирует плагин в
# <DigitQt.exe>/plugins/tracers/.
add_custom_command(TARGET scanline_extremum_tracker_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DigitQt>/plugins/tracers"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:scanline_extremum_tracker_plugin>"
            "$<TARGET_FILE_DIR:DigitQt>/plugins/tracers/"
)
