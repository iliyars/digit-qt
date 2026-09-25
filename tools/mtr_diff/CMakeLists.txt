# Диагностический CLI-инструмент, не часть приложения -- собирается
# только при -DDIGITQT_BUILD_TOOLS=ON. Линкует DigitQt::Core (см.
# src/core/CMakeLists.txt) вместо повторной компиляции core/-исходников;
# io/MtrImporter.cpp и io/ModalReportExporter.cpp остаются вне core (это
# Qt-пограничный слой) и компилируются сюда напрямую, как и раньше.

add_executable(mtr_diff
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/io/MtrImporter.cpp
    ${CMAKE_SOURCE_DIR}/src/io/ModalReportExporter.cpp
)

target_include_directories(mtr_diff PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(mtr_diff PRIVATE Qt5::Core Qt5::Gui DigitQt::Core)

set_target_properties(mtr_diff PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
