# Хост-инструмент для проверки PoC-плагина (см. ../binary_thinning_poc):
# грузит DLL через src/plugin_host/PluginFringeTracer (та же обёртка,
# которую использует остальное приложение) и сверяет результат со
# встроенным трекером. Не часть приложения -- собирается только при
# -DDIGITQT_BUILD_PLUGIN_POC=ON.

add_executable(plugin_tracer_check
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/io/ImageLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin_host/PluginFringeTracer.cpp
)

target_include_directories(plugin_tracer_check PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(plugin_tracer_check PRIVATE Qt5::Core Qt5::Gui DigitQt::Core
                                                  DigitQt::PluginAbi)

set_target_properties(plugin_tracer_check PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
