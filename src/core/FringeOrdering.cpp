#include "FringeOrdering.h"

#include <algorithm>
#include <numeric>
#include <cmath>
#include <map>
#include <set>

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

  // Mean X is not an ordering coordinate for curved or horizontal
  // fringes: their ends are clipped differently by the pupil. Infer
  // adjacency from intersections with common sections instead.
  if (lines.size() < 2)
    return;
  double tangentX = 0, tangentY = 0;
  size_t anchor = 0;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (lines[i].points.size() > lines[anchor].points.size())
      anchor = i;
    const auto &p = lines[i].points;
    for (size_t j = 1; j < p.size(); ++j) {
      const double dx = p[j].x - p[j - 1].x, dy = p[j].y - p[j - 1].y;
      const double length2 = dx * dx + dy * dy;
      if (length2 > 0) {
        tangentX += dx * dx / length2;
        tangentY += dy * dy / length2;
      }
    }
  }
  const bool columns = tangentX > tangentY;
  std::map<int, std::map<size_t, double>> sections;
  std::set<int> ambiguous;
  for (size_t i = 0; i < lines.size(); ++i) {
    const auto &p = lines[i].points;
    for (size_t j = 1; j < p.size(); ++j) {
      const double a = columns ? p[j - 1].x : p[j - 1].y;
      const double b = columns ? p[j].x : p[j].y;
      if (!std::isfinite(a) || !std::isfinite(b) || std::abs(b - a) < 1e-9)
        continue;
      for (int section = int(std::ceil(std::min(a, b))); section <= std::floor(std::max(a, b));
           ++section) {
        const double t = (section - a) / (b - a);
        const double cross = columns ? p[j - 1].y + t * (p[j].y - p[j - 1].y)
                                     : p[j - 1].x + t * (p[j].x - p[j - 1].x);
        const auto inserted = sections[section].emplace(i, cross);
        if (!inserted.second && std::abs(inserted.first->second - cross) > 1)
          ambiguous.insert(section);  // Closed/folded curves have no single crossing.
      }
    }
  }
  std::map<std::pair<size_t, size_t>, int> votes;
  for (const auto &[section, crossings] : sections) {
    if (ambiguous.count(section))
      continue;
    std::vector<std::pair<double, size_t>> ordered;
    for (const auto &[index, cross] : crossings)
      ordered.emplace_back(cross, index);
    std::sort(ordered.begin(), ordered.end());
    for (size_t i = 1; i < ordered.size(); ++i)
      ++votes[{ordered[i - 1].second, ordered[i].second}];
  }
  std::vector<double> orders(lines.size());
  std::vector<bool> assigned(lines.size(), false);
  orders[anchor] = 0;
  assigned[anchor] = true;
  for (;;) {
    int best = 0;
    size_t next = 0;
    double order = 0;
    for (const auto &[edge, count] : votes) {
      const auto [a, b] = edge;
      if (count <= best || assigned[a] == assigned[b])
        continue;
      best = count;
      next = assigned[a] ? b : a;
      order = assigned[a] ? orders[a] + 1 : orders[b] - 1;
    }
    if (!best)
      break;
    orders[next] = order;
    assigned[next] = true;
  }
  // Keep the historical fallback if topology does not connect all lines;
  // disconnected and closed contours need explicit order information.
  if (std::find(assigned.begin(), assigned.end(), false) != assigned.end())
    return;
  const double lowest = *std::min_element(orders.begin(), orders.end());
  for (size_t i = 0; i < lines.size(); ++i)
    if (!lines[i].orderIsManual)
      lines[i].order = orders[i] - lowest;
}

}  // namespace digitqt::core
