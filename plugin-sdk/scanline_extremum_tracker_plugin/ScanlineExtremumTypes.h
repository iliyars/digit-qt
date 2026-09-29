// Порт core/pipeline/stages/fringe_tracing/scanline_extremum/ScanlineExtremumTypes.h
// без изменений -- уже был Qt/core-свободным (работает поверх сырых
// указателей на пиксели), только пространство имён поменяно, чтобы не
// создавать видимость зависимости от DigitQt::Core.
#pragma once

#include <functional>
#include <vector>

namespace scanline_extremum_plugin {

struct Point2d {
  double x{0.0};
  double y{0.0};
};

struct Limits {
  Point2d leftEdge;
  Point2d rightEdge;
};

enum class ExtremumType { Red = 0, Black = 1 };

enum class FringeCenterMode { Max = 0, Min = 1, MinMax = 2 };

struct ExtremumPoint {
  Point2d position;
  double intensity{0.0};
  ExtremumType extremumType{ExtremumType::Red};
  bool assigned{false};
  double number{-1000.0};
  Limits window{{0.0, 0.0}, {0.0, 0.0}};
  int chainId{-1};
};

struct Section {
  std::vector<ExtremumPoint> points;
  Limits limits{{0.0, 0.0}, {0.0, 0.0}};
  double averageStep{0.0};
};

struct NumberedFringe {
  std::vector<Point2d> points;
  double number{0.0};
  int segmentIndex{-1};
};

struct DigitizationInput {
  const unsigned char *bitmapData{nullptr};
  int imageWidth{0};
  int imageHeight{0};
  int bytesPerLine{0};

  std::function<bool(int, int)> isVisible;

  FringeCenterMode fringeCenterAs{FringeCenterMode::MinMax};
  double fringeStep{1.0};
  double toleranceFactor{0.3};
};

}  // namespace scanline_extremum_plugin
