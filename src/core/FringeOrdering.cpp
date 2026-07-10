#include "FringeOrdering.h"

#include <algorithm>
#include <numeric>

namespace digitqt::core {

namespace {

double averageX(const tracing::TracedLine &points) {
  if (points.empty())
    return 0.0;
  double sum = 0.0;
  for (const auto &p : points)
    sum += p.x;
  return sum / static_cast<double>(points.size());
}
}  // namespace

void autoAssignFringeOrder(std::vector<NumberedFringeLine> &lines) {
  std::vector<size_t> indices(lines.size());
  std::iota(indices.begin(), indices.end(), size_t{0});

  std::sort(indices.begin(), indices.end(), [&lines](size_t a, size_t b) {
    return averageX(lines[a].points) < averageX(lines[b].points);
  });

  double nextOrder = 0.0;
  for (const size_t idx : indices) {
    if (!lines[idx].orderIsManual)
      lines[idx].order = nextOrder;
    nextOrder += 1.0;
  }
}

}  // namespace digitqt::core
