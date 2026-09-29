# Полностью самодостаточный плагин: горизонтальная сплайн-интерполяция
# реализована целиком внутри plugin.cpp, никакого DigitQt::Core -- см.
# sequential_fringe_tracker_plugin/CMakeLists.txt (та же выноска).

add_library(horizontal_spline_phase_plugin MODULE
    plugin.cpp
)

target_link_libraries(horizontal_spline_phase_plugin PRIVATE DigitQt::PluginAbi)

set_target_properties(horizontal_spline_phase_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    PREFIX ""
    OUTPUT_NAME "horizontal_spline"
)

# См. plugin-sdk/binary_thinning_poc/CMakeLists.txt -- копирует плагин в
# <DigitQt.exe>/plugins/phase/.
add_custom_command(TARGET horizontal_spline_phase_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DigitQt>/plugins/phase"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:horizontal_spline_phase_plugin>"
            "$<TARGET_FILE_DIR:DigitQt>/plugins/phase/"
)
