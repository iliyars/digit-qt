# Диагностический CLI-инструмент (см. main.cpp) -- собирается только при
# -DDIGITQT_BUILD_TOOLS=ON. Как terminal_pipeline: DigitQt::Core и
# DigitQt::Formats линкуются, io/ImageLoader.cpp (Qt-пограничный слой)
# компилируется сюда напрямую.

add_executable(psi_check
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/io/ImageLoader.cpp
)

target_include_directories(psi_check PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(psi_check PRIVATE Qt5::Core Qt5::Gui DigitQt::Core DigitQt::Formats)

set_target_properties(psi_check PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
