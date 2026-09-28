# Публичный ABI-контракт плагинов -- ничего не знает про src/ (см.
# include/dqt_fringe_tracer_abi.h, чистый C). Оформлен header-only
# INTERFACE-таргетом, чтобы и PoC-плагин, и хост-инструмент проверки
# подключали один и тот же include-путь одинаковым образом.
add_library(dqt_plugin_abi INTERFACE)
target_include_directories(dqt_plugin_abi INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(DigitQt::PluginAbi ALIAS dqt_plugin_abi)

# binary_thinning_poc/ и plugin_tracer_check/ -- это уже не сам ABI, а
# проверка его на практике (см. обсуждение); оба зависят от DigitQt::Core,
# поэтому собираются только по явному флагу, а не всегда.
if(DIGITQT_BUILD_PLUGIN_POC)
    add_subdirectory(binary_thinning_poc)
    add_subdirectory(plugin_tracer_check)
endif()
