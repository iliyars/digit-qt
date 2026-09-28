# Сквозная проверка core::tracing::tryLoadTracerPlugin() (реального
# пути, которым пользуется SetupStage) для всех 4 трекеров сразу --
# в отличие от plugin_tracer_check (один конкретный .dll путь на входе).

add_executable(tracer_plugin_selftest
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/io/ImageLoader.cpp
)

target_include_directories(tracer_plugin_selftest PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(tracer_plugin_selftest PRIVATE Qt5::Core Qt5::Gui DigitQt::Core
                                                      DigitQt::PluginAbi)

set_target_properties(tracer_plugin_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    # Та же директория, что и у DigitQt.exe -- так
    # core::plugin_loading::pluginsDirectory() (GetModuleFileNameW этого
    # процесса) находит тот же build/.../plugins/tracers/, куда
    # POST_BUILD-шаги плагинов копируют .dll (см. ../binary_thinning_poc/
    # CMakeLists.txt).
    RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:DigitQt>"
)
