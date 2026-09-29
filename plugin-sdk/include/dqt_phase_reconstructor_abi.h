#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define DQT_ABI_EXPORT __declspec(dllexport)
#else
#define DQT_ABI_EXPORT __attribute__((visibility("default")))
#endif

#define DQT_PHASE_RECONSTRUCTOR_ABI_VERSION 2u

/* --- Данные: заимствованные, только для чтения представления --- */

typedef struct {
  int32_t width;
  int32_t height;
  const uint8_t *pixels; /* row-major, stride == width */
} DqtPhaseBitmapView;

/* Одна точка линии полосы -- геометрия ТОЛЬКО, без width/intensity:
   сшивке фазы не нужны эти поля из DqtTracedPoint, тащить их сюда --
   лишняя связанность с контрактом трассировки. */
typedef struct {
  double x;
  double y;
} DqtPhasePoint;

typedef struct {
  const DqtPhasePoint *points;
  size_t count;
  double order; /* номер полосы, см. NumberedFringeLine::order */
} DqtPhaseNumberedFringeLine;

typedef int (*DqtPhaseVisibilityFn)(int32_t x, int32_t y, void *userData);

/* --- Контракт узла (соответствует core::IPhaseReconstructor) --- */

typedef struct DqtPhaseReconstructorImpl *DqtPhaseReconstructorHandle;

typedef struct DqtPhaseReconstructorVTable {
  DqtPhaseReconstructorHandle (*create)(void);
  void (*destroy)(DqtPhaseReconstructorHandle self);

  /* Один вызов на всё -- в C++-интерфейсе тоже нет отдельного
     initialize(), незачем изобретать его здесь (см. заметки к шагу 0).

     image -- может быть проигнорирован реализацией (HorizontalSpline),
     но хост передаёт его всегда, не зная заранее, нужен ли он.
     isVisible -- предикат в координатах (gridWidth x gridHeight).
     lines/lineCount -- пронумерованные линии в тех же координатах;
     реализациям, которым не нужны, передаётся lineCount=0.

     outValues -- буфер на gridWidth*gridHeight double, ВЫДЕЛЕННЫЙ
     ХОСТОМ заранее; NaN = "вне апертуры/не вычислено". Плагин только
     заполняет -- freeXxx() не нужен вообще.

     Возвращает 0 при неудаче (см. lastError()), не 0 при успехе. */
  int (*reconstruct)(DqtPhaseReconstructorHandle self, int32_t gridWidth, int32_t gridHeight,
                     const DqtPhaseBitmapView *image, DqtPhaseVisibilityFn isVisible,
                     void *isVisibleUserData, const DqtPhaseNumberedFringeLine *lines,
                     size_t lineCount, double *outValues);

  const char *(*name)(DqtPhaseReconstructorHandle self);
  const char *(*lastError)(DqtPhaseReconstructorHandle self);
} DqtPhaseReconstructorVTable;

typedef struct {
  const char *pluginName;
  const char *pluginVersion;
  int32_t needsFringeLines;  // 0 - линии трассировки не нужны, 1 - линии трассировки нужны.
} DqtPhasePluginInfo;

typedef int (*DqtPhasePluginEntryFn)(uint32_t hostAbiVersion, DqtPhasePluginInfo *outInfo,
                                     const DqtPhaseReconstructorVTable **outVTable);

#ifdef __cplusplus
}
#endif
