# Только MiddleAlgorithm.{h,cpp} -- см. комментарий в MiddleAlgorithm.h
# для объяснения, почему это не вернуло весь ScanlineExtremumTracker
# обратно в core.
target_sources(core
    PRIVATE
        MiddleAlgorithm.h
        MiddleAlgorithm.cpp
)
