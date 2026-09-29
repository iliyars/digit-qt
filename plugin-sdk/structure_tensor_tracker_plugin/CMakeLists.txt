# Полностью самодостаточный плагин: StructureTensorTracker реализован
# целиком внутри plugin.cpp, никакого DigitQt::Core -- см.
# sequential_fringe_tracker_plugin/CMakeLists.txt (та же выноска).
#
# ВАЖНО: у SetupStage больше нет встроенной реализации этого алгоритма --
# без этого .dll рядом с DigitQt.exe выбор Structure Tensor в UI
# завершится ошибкой, а не тихим fallback'ом.

add_library(structure_tensor_tracker_plugin MODULE
    plugin.cpp
)

target_link_libraries(structure_tensor_tracker_plugin PRIVATE DigitQt::PluginAbi)

set_target_properties(structure_tensor_tracker_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    PREFIX ""
    OUTPUT_NAME "structure_tensor_tracker"
)

# См. binary_thinning_poc/CMakeLists.txt -- копирует плагин в
# <DigitQt.exe>/plugins/tracers/.
add_custom_command(TARGET structure_tensor_tracker_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DigitQt>/plugins/tracers"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:structure_tensor_tracker_plugin>"
            "$<TARGET_FILE_DIR:DigitQt>/plugins/tracers/"
)
