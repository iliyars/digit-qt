# Ни одна встроенная реализация IPhaseReconstructor не осталась -- все 3
# метода (HorizontalSpline/Fourier/Wavelet) вынесены как самодостаточные
# DqtPhaseReconstructor C ABI-плагины в plugin-sdk/*_phase_plugin/.
# PhaseReconstructionStage.cpp грузит их через PhasePluginLoading.h;
# отсутствие нужного .dll -- настоящая ошибка, не fallback (см. ту же
# историю для трассировки в памяти qt_decoupling_and_plugin_abi).
target_sources(core
    PRIVATE
        IPhaseReconstructor.h
        DllPhaseReconstructor.h
        DllPhaseReconstructor.cpp
        PhasePluginLoading.h
        PhasePluginLoading.cpp
)
