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
)
