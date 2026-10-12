# DigitQtFormats -- адаптеры внешних форматов файлов (см.
# notes/architecture.md, раздел 11): каждый переводит содержимое файла в
# срез канонической модели из DigitQt::Core и отчитывается, что при этом
# потеряно или преобразовано.
#
# Как и core, не зависит от Qt -- и так же гарантирует это тем, что Qt5 к
# этой цели просто не линкуется (см. src/core/CMakeLists.txt). Старый
# Qt-слой src/io/ пока живёт рядом, форматы переезжают сюда по одному.
add_library(formats STATIC
    FormatAdapter.h
    TextParsing.h
    TextParsing.cpp
    MtrAdapter.h
    MtrAdapter.cpp
    FrnAdapter.h
    FrnAdapter.cpp
    FormatRegistry.h
    FormatRegistry.cpp
)
add_library(DigitQt::Formats ALIAS formats)

target_link_libraries(formats PUBLIC DigitQt::Core)

target_compile_features(formats PUBLIC cxx_std_17)

set_target_properties(formats PROPERTIES
    AUTOMOC OFF
    AUTORCC OFF
    AUTOUIC OFF
)
