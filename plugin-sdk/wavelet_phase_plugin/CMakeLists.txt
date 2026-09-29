# Полностью самодостаточный плагин: метод непрерывного вейвлет-анализа
# реализован целиком внутри plugin.cpp. Линкует OpenCV напрямую (только
# core -- CWT здесь не использует imgproc/DIST_L2, в отличие от Fourier-
# и BinaryThinning-плагинов), но НЕ DigitQt::Core -- см.
# fourier_phase_plugin/CMakeLists.txt (та же выноска).

add_library(wavelet_phase_plugin MODULE
    plugin.cpp
)

target_include_directories(wavelet_phase_plugin PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(wavelet_phase_plugin PRIVATE DigitQt::PluginAbi ${OpenCV_LIBS})

set_target_properties(wavelet_phase_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    PREFIX ""
    OUTPUT_NAME "wavelet"
)

# См. fourier_phase_plugin/CMakeLists.txt -- копирует плагин в
# <DigitQt.exe>/plugins/phase/.
add_custom_command(TARGET wavelet_phase_plugin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:DigitQt>/plugins/phase"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:wavelet_phase_plugin>"
            "$<TARGET_FILE_DIR:DigitQt>/plugins/phase/"
)
