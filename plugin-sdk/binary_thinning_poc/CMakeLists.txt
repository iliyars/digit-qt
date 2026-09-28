# Proof-of-concept плагин, проверяющий DqtFringeTracer C ABI (см.
# plugin-sdk/include/dqt_fringe_tracer_abi.h). Только для этого PoC
# линкует DigitQt::Core напрямую, чтобы переиспользовать настоящий
# BinaryThinningTracker вместо повторной реализации алгоритма -- реальный
# сторонний плагин так делать не должен (core -- C++ библиотека без
# гарантии ABI между компиляторами), он принёс бы свою реализацию. Смысл
# здесь -- проверить механику границы (владение памятью, версионирование,
# DLL), а не поставить готовый плагин.

add_library(binary_thinning_poc_plugin MODULE
    plugin.cpp
)

target_link_libraries(binary_thinning_poc_plugin PRIVATE DigitQt::Core DigitQt::PluginAbi)

set_target_properties(binary_thinning_poc_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    PREFIX ""
    # Реальное имя файла плагина, которое ищет TracerPluginLoading.cpp
    # (<каталог exe>/plugins/tracers/binary_thinning_tracker.dll) -- этот
    # PoC-таргет со временем стал не только проверкой ABI, но и реальным
    # плагином для BinaryThinningTracker.
    OUTPUT_NAME "binary_thinning_tracker"
)

# Копирует плагин рядом с DigitQt.exe, в plugins/tracers/ -- именно там
# его ищет core::tracing::tryLoadTracerPlugin() (см.
# src/core/pipeline/stages/fringe_tracing/TracerPluginLoading.cpp). Без
# этого шага DIGITQT_BUILD_PLUGIN_POC=ON собирает плагин, но реальный
# DigitQt.exe его не найдёт и тихо продолжит работать на встроенном
# трекере.
add_custom_command(TARGET binary_thinning_poc_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DigitQt>/plugins/tracers"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:binary_thinning_poc_plugin>"
            "$<TARGET_FILE_DIR:DigitQt>/plugins/tracers/"
)
