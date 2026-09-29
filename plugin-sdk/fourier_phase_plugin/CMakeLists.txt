# Полностью самодостаточный плагин: метод Фурье-анализа полос реализован
# целиком внутри plugin.cpp. Линкует OpenCV напрямую (сторонняя
# библиотека, естественная зависимость самого алгоритма -- БПФ,
# distanceTransform), но НЕ DigitQt::Core -- см.
# horizontal_spline_phase_plugin/CMakeLists.txt (та же выноска) и
# binary_thinning_tracker_plugin/CMakeLists.txt (тот же приём с OpenCV).

add_library(fourier_phase_plugin MODULE
    plugin.cpp
)

target_include_directories(fourier_phase_plugin PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(fourier_phase_plugin PRIVATE DigitQt::PluginAbi ${OpenCV_LIBS})

set_target_properties(fourier_phase_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    PREFIX ""
    OUTPUT_NAME "fourier"
)

# См. horizontal_spline_phase_plugin/CMakeLists.txt -- копирует плагин в
# <DigitQt.exe>/plugins/phase/.
add_custom_command(TARGET fourier_phase_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DigitQt>/plugins/phase"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:fourier_phase_plugin>"
            "$<TARGET_FILE_DIR:DigitQt>/plugins/phase/"
)
