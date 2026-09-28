# Полностью самодостаточный плагин: SequentialFringeTracker реализован
# целиком внутри plugin.cpp, никакого DigitQt::Core -- см. комментарий в
# начале plugin.cpp. Собирается и копируется рядом с DigitQt.exe (см.
# top-level CMakeLists.txt) только при -DDIGITQT_BUILD_PLUGIN_POC=ON --
# сборка плагинов отдельным шагом ещё не оформлена как часть обычной
# сборки (см. заметку про dist/ в build_and_test).
#
# ВАЖНО: у SetupStage больше нет встроенной реализации этого алгоритма
# (см. SetupStage.cpp) -- без этого .dll рядом с DigitQt.exe выбор
# Sequential Tracking в UI завершится ошибкой, а не тихим fallback'ом.

add_library(sequential_fringe_tracker_plugin MODULE
    plugin.cpp
)

target_link_libraries(sequential_fringe_tracker_plugin PRIVATE DigitQt::PluginAbi)

set_target_properties(sequential_fringe_tracker_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    PREFIX ""
    OUTPUT_NAME "sequential_fringe_tracker"
)

# См. binary_thinning_poc/CMakeLists.txt -- копирует плагин в
# <DigitQt.exe>/plugins/tracers/.
add_custom_command(TARGET sequential_fringe_tracker_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DigitQt>/plugins/tracers"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:sequential_fringe_tracker_plugin>"
            "$<TARGET_FILE_DIR:DigitQt>/plugins/tracers/"
)
