#include "PhaseUnwrapping.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <vector>

namespace digitqt::core {

namespace {

struct Edge {
  double quality;
  int32_t from;
  int32_t to;
  bool operator<(const Edge &other) const { return quality < other.quality; }  // max-heap
};

}  // namespace

bool unwrapPhase(const PhaseMap &wrapped, const PhaseMap &quality, UnwrapResult &out,
                 std::string &error) {
  const int width = wrapped.width();
  const int height = wrapped.height();
  if (wrapped.isEmpty()) {
    error = "Nothing to unwrap: the phase map is empty";
    return false;
  }
  const bool hasQuality = !quality.isEmpty();
  if (hasQuality && (quality.width() != width || quality.height() != height)) {
    error = "Quality map size does not match the phase map";
    return false;
  }

  const double twoPi = 2.0 * std::acos(-1.0);
  const size_t total = static_cast<size_t>(width) * static_cast<size_t>(height);
  auto qualityAt = [&](int32_t index) {
    return hasQuality ? quality.value(index % width, index / width) : 1.0;
  };

  // Точки -- кандидаты в начало новой области, от самой надёжной.
  std::vector<int32_t> seeds;
  for (int32_t index = 0; index < static_cast<int32_t>(total); ++index)
    if (wrapped.hasValue(index % width, index / width))
      seeds.push_back(index);
  if (seeds.empty()) {
    error = "Nothing to unwrap: no valid phase points";
    return false;
  }
  std::stable_sort(seeds.begin(), seeds.end(),
                   [&](int32_t a, int32_t b) { return qualityAt(a) > qualityAt(b); });

  std::vector<double> unwrapped(total, std::numeric_limits<double>::quiet_NaN());
  std::vector<uint8_t> done(total, 0);
  std::priority_queue<Edge> edges;

  auto pushNeighbours = [&](int32_t index) {
    const int x = index % width, y = index / width;
    const double q = qualityAt(index);
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; ++k) {
      const int nx = x + dx[k], ny = y + dy[k];
      if (nx < 0 || nx >= width || ny < 0 || ny >= height)
        continue;
      const int32_t neighbour = ny * width + nx;
      if (done[neighbour] || !wrapped.hasValue(nx, ny))
        continue;
      edges.push({std::min(q, qualityAt(neighbour)), index, neighbour});
    }
  };

  int regions = 0;
  for (const int32_t seed : seeds) {
    if (done[seed])
      continue;
    ++regions;
    unwrapped[seed] = wrapped.value(seed % width, seed / width);
    done[seed] = 1;
    pushNeighbours(seed);

    while (!edges.empty()) {
      const Edge edge = edges.top();
      edges.pop();
      if (done[edge.to])
        continue;
      const double w = wrapped.value(edge.to % width, edge.to / width);
      // Из w + 2*pi*k -- ближайшее к уже развёрнутому соседу.
      unwrapped[edge.to] = w + twoPi * std::round((unwrapped[edge.from] - w) / twoPi);
      done[edge.to] = 1;
      pushNeighbours(edge.to);
    }
  }

  PhaseMap orders(width, height);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      if (done[static_cast<size_t>(y) * width + x])
        orders.setValue(x, y, unwrapped[static_cast<size_t>(y) * width + x] / twoPi);

  out.orders = std::move(orders);
  out.regionCount = regions;
  return true;
}

}  // namespace digitqt::core
