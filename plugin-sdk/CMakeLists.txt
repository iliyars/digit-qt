# Публичный ABI-контракт плагинов -- ничего не знает про src/ (см.
# include/dqt_fringe_tracer_abi.h, чистый C). Оформлен header-only
# INTERFACE-таргетом, чтобы PoC-плагин, хост-инструмент проверки и
# src/plugin_host/ подключали один и тот же include-путь одинаковым
# образом.
#
# Только этот таргет; binary_thinning_poc/ и plugin_tracer_check/
# подключаются отдельно из верхнеуровневого CMakeLists.txt ПОСЛЕ src/
# (им нужен DigitQt::Core) -- а этот файл, наоборот, должен отработать
# ДО src/ (src/plugin_host/ нужен DigitQt::PluginAbi).
add_library(dqt_plugin_abi INTERFACE)
target_include_directories(dqt_plugin_abi INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(DigitQt::PluginAbi ALIAS dqt_plugin_abi)
