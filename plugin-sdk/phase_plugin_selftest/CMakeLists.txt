# Сквозная проверка core::tryLoadPhaseReconstructorPlugin() (реального
# пути, которым пользуется PhaseReconstructionStage) для всех 3 методов
# сшивки фазы -- сверяет с ещё существующими built-in реализациями.

add_executable(phase_plugin_selftest
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/io/ImageLoader.cpp
)

target_include_directories(phase_plugin_selftest PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(phase_plugin_selftest PRIVATE Qt5::Core Qt5::Gui DigitQt::Core
                                                     DigitQt::PluginAbi)

set_target_properties(phase_plugin_selftest PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    # Та же директория, что и у DigitQt.exe -- см.
    # tracer_plugin_selftest/CMakeLists.txt (тот же приём).
    RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:DigitQt>"
)
