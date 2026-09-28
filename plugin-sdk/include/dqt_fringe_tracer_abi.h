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

#define DQT_FRINGE_TRACER_ABI_VERSION 1u

/* --- Данные: заимствованные, только для чтения представления --- */
/* Хост владеет памятью на всё время вызова; плагин не должен её освобождать. */

typedef struct {
  int32_t width;
  int32_t height;
  const uint8_t *pixels; /* row-major, stride == width */
} DqtBitmapView;

typedef struct {
  int32_t x;
  int32_t y;
} DqtSeedPoint;

typedef struct {
  double x;
  double y;
  float width;
  float intensity;
} DqtTracedPoint;

/* Владеет ПЛАГИН, пока не будет вызван freeLines() с теми же указателями. */
typedef struct {
  const DqtTracedPoint *points;
  size_t count;
} DqtTracedLine;

/* Предикат видимости -- замена std::function<bool(int,int)> на границе.
   userData -- то же самое, что хост передал в initialize(). */
typedef int (*DqtVisibilityFn)(int32_t x, int32_t y, void *userData);

/* --- Контракт узла (соответствует core::tracing::IFringeTracer) --- */

typedef struct DqtFringeTracerImpl *DqtFringeTracerHandle; /* непрозрачный */

typedef struct {
  DqtFringeTracerHandle (*create)(void);
  void (*destroy)(DqtFringeTracerHandle self);

  /* 0 при неудаче (см. lastError()), не 0 при успехе. */
  int (*initialize)(DqtFringeTracerHandle self, const DqtBitmapView *image,
                    DqtVisibilityFn isVisible, void *isVisibleUserData);

  /* При успехе *outLines/*outLineCount описывают результат; плагин
     сохраняет владение до вызова freeLines() с теми же указателями.
     При неудаче *outLines = NULL. */
  int (*extract)(DqtFringeTracerHandle self, const DqtSeedPoint *seeds, size_t seedCount,
                 DqtTracedLine **outLines, size_t *outLineCount);

  void (*freeLines)(DqtFringeTracerHandle self, DqtTracedLine *lines, size_t lineCount);

  /* Возвращаемые строки принадлежат плагину/self и валидны до следующего
     вызова на этом handle либо до destroy(). */
  const char *(*name)(DqtFringeTracerHandle self);
  const char *(*lastError)(DqtFringeTracerHandle self);
} DqtFringeTracerVTable;

typedef struct {
  const char *pluginName;
  const char *pluginVersion;
} DqtPluginInfo;

/* Каждая DLL-плагин экспортирует ровно этот символ под именем
   "dqt_plugin_entry", чтобы хост мог его найти (GetProcAddress/
   QLibrary::resolve), не зная заранее имени плагина.

   hostAbiVersion -- DQT_FRINGE_TRACER_ABI_VERSION, с которой собран ХОСТ.
   Плагин обязан сверить её со своей версией и отказаться (вернуть 0)
   при несовпадении, а не гадать. */
typedef int (*DqtPluginEntryFn)(uint32_t hostAbiVersion, DqtPluginInfo *outInfo,
                                const DqtFringeTracerVTable **outVTable);

#ifdef __cplusplus
}
#endif
