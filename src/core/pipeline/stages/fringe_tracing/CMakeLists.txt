target_sources(core
    PRIVATE
        IFringeTracer.h
        StructureTensorTracker.h
        StructureTensorTracker.cpp
        ScanlineExtremumTracker.h
        ScanlineExtremumTracker.cpp
        BinaryThinningTracker.h
        BinaryThinningTracker.cpp
        DllFringeTracer.h
        DllFringeTracer.cpp
        TracerPluginLoading.h
        TracerPluginLoading.cpp
)

add_subdirectory(scanline_extremum)
