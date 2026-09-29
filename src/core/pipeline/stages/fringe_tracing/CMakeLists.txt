# Ни один встроенный IFringeTracer не остался -- все 4 алгоритма (см.
# core::TracerAlgorithm) вынесены как самодостаточные DqtFringeTracer C
# ABI-плагины в plugin-sdk/*_tracker_plugin/. SetupStage.cpp грузит их
# через TracerPluginLoading.h; отсутствие нужного .dll -- настоящая
# ошибка, не fallback (см. историю в памяти qt_decoupling_and_plugin_abi).
target_sources(core
    PRIVATE
        IFringeTracer.h
        DllFringeTracer.h
        DllFringeTracer.cpp
        TracerPluginLoading.h
        TracerPluginLoading.cpp
)

# scanline_extremum/ теперь содержит только MiddleAlgorithm.{h,cpp},
# переиспользуемый AutoSeedPlacement.cpp -- см. её CMakeLists.txt.
add_subdirectory(scanline_extremum)
