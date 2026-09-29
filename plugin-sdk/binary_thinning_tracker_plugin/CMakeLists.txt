# Полностью самодостаточный плагин: BinaryThinningTracker реализован
# целиком внутри plugin.cpp. Линкует OpenCV напрямую (сторонняя
# библиотека, естественная зависимость самого алгоритма -- adaptive
# threshold/morphology/skeletonization), но НЕ DigitQt::Core -- см.
# sequential_fringe_tracker_plugin/CMakeLists.txt (та же выноска).
#
# Заменяет старый plugin-sdk/binary_thinning_poc/ (тонкая обёртка вокруг
# core::tracing::BinaryThinningTracker) -- см. top-level CMakeLists.txt.
#
# ВАЖНО: у SetupStage больше нет встроенной реализации этого алгоритма --
# без этого .dll рядом с DigitQt.exe выбор Binary Thinning в UI
# завершится ошибкой, а не тихим fallback'ом.

add_library(binary_thinning_tracker_plugin MODULE
    plugin.cpp
)

target_include_directories(binary_thinning_tracker_plugin PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(binary_thinning_tracker_plugin PRIVATE DigitQt::PluginAbi ${OpenCV_LIBS})

set_target_properties(binary_thinning_tracker_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    PREFIX ""
    OUTPUT_NAME "binary_thinning_tracker"
)

# См. предыдущие плагины -- копирует .dll в <DigitQt.exe>/plugins/tracers/.
add_custom_command(TARGET binary_thinning_tracker_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DigitQt>/plugins/tracers"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:binary_thinning_tracker_plugin>"
            "$<TARGET_FILE_DIR:DigitQt>/plugins/tracers/"
)
