# Диагностический CLI-инструмент (см. main.cpp) -- собирается только при
# -DDIGITQT_BUILD_TOOLS=ON. Линкует DigitQt::Core (см.
# src/core/CMakeLists.txt) вместо повторной компиляции core/-исходников;
# io/ImageLoader.cpp и io/MtrExporter.cpp остаются вне core (это
# Qt-пограничный слой) и компилируются сюда напрямую, как и раньше.

add_executable(terminal_pipeline
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/io/ImageLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/io/MtrExporter.cpp
)

target_include_directories(terminal_pipeline PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(terminal_pipeline PRIVATE Qt5::Core Qt5::Gui DigitQt::Core)

set_target_properties(terminal_pipeline PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
