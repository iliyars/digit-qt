#include "MtrAdapter.h"

#include "formats/TextParsing.h"

#include <charconv>
#include <cmath>

namespace digitqt::formats {

using text::lineValue;
using text::parseDouble;
using text::Tokenizer;

namespace {

constexpr std::string_view kMatrixTag = "[MATRIX]";

}  // namespace

std::string MtrAdapter::displayName() const { return "WinFringe Matrix"; }

std::vector<std::string> MtrAdapter::extensions() const { return {"mtr"}; }

bool MtrAdapter::probe(std::string_view head) const {
  return lineValue(head, "Size=").has_value() && lineValue(head, kMatrixTag).has_value();
}

bool MtrAdapter::importData(std::string_view data, ImportedData &out, std::string &error) const {
  const auto sizeValue = lineValue(data, "Size=");
  int n = 0;
  if (!sizeValue ||
      std::from_chars(sizeValue->data(), sizeValue->data() + sizeValue->size(), n).ec != std::errc()) {
    error = "Missing or invalid Size= header";
    return false;
  }
  if (n < 3) {
    error = "Invalid matrix size: " + std::to_string(n);
    return false;
  }

  const size_t matrixTag = data.find(kMatrixTag);
  if (matrixTag == std::string_view::npos) {
    error = "Missing [MATRIX] section";
    return false;
  }
  const size_t dataStart = matrixTag + kMatrixTag.size();
  const size_t endTag = data.find("END", dataStart);
  const std::string_view matrixSection =
      data.substr(dataStart, endTag == std::string_view::npos ? endTag : endTag - dataStart);

  core::PhaseMap phase(n, n);
  const double ratio = 2.0 / static_cast<double>(n - 1);
  const double center = (n - 1) / 2.0;

  int stored = 0, outsideGrid = 0, nonFinite = 0, overwritten = 0;

  // Tokens are: Y (start of a row), then "Z X" pairs, then "E" (end of
  // row) -- regardless of how export wrapped them across physical lines.
  Tokenizer tokens(matrixSection);
  std::string_view token;
  while (tokens.next(token)) {
    double yNorm = 0.0;
    if (!parseDouble(token, yNorm)) {
      error = "Malformed row: expected a Y value at token " + std::to_string(tokens.index());
      return false;
    }
    const int row = static_cast<int>(std::lround(center - yNorm / ratio));

    while (tokens.next(token) && token != "E") {
      const int pairIndex = tokens.index();
      std::string_view xToken;
      if (!tokens.next(xToken)) {
        error = "Truncated row: a Z value is missing its X";
        return false;
      }
      double z = 0.0, xNorm = 0.0;
      if (!parseDouble(token, z) || !parseDouble(xToken, xNorm)) {
        error = "Malformed Z/X pair at token " + std::to_string(pairIndex);
        return false;
      }

      if (!std::isfinite(z)) {
        ++nonFinite;
        continue;
      }
      const int col = static_cast<int>(std::lround(xNorm / ratio + center));
      if (col < 0 || col >= n || row < 0 || row >= n) {
        ++outsideGrid;
        continue;
      }
      if (phase.hasValue(col, row))
        ++overwritten;
      else
        ++stored;
      phase.setValue(col, row, z);
    }
  }

  if (stored == 0) {
    error = "No data points found in the matrix";
    return false;
  }

  const std::string_view header = data.substr(0, matrixTag);
  const std::string grid = std::to_string(n) + "x" + std::to_string(n);
  out.report.push_back({Fidelity::Converted, "Normalized [-1, 1] coordinates placed on a new " + grid +
                                                 " grid; the map is not tied to any image"});

  const auto units = lineValue(header, "Units=");
  if (!units)
    out.report.push_back({Fidelity::Missing, "No Units= header; values taken as waves"});
  else if (*units == "WAV")
    out.report.push_back({Fidelity::Lossless, "Units=WAV: values taken as waves, unchanged"});
  else
    out.report.push_back({Fidelity::Dropped, "Units=" + std::string(*units) +
                                                 " is not supported; values taken as waves, unchanged"});

  if (lineValue(header, "Title=") || lineValue(header, "Date=") || lineValue(header, "Time="))
    out.report.push_back({Fidelity::Dropped, "Title/Date/Time headers are not kept"});
  if (outsideGrid > 0)
    out.report.push_back(
        {Fidelity::Dropped, std::to_string(outsideGrid) + " point(s) outside the " + grid + " grid"});
  if (nonFinite > 0)
    out.report.push_back({Fidelity::Dropped, std::to_string(nonFinite) + " NaN/Inf value(s)"});
  if (overwritten > 0)
    out.report.push_back({Fidelity::Converted, std::to_string(overwritten) +
                                                   " point(s) rounded onto an already filled cell; "
                                                   "the later value was kept"});

  out.phaseMap = std::move(phase);
  return true;
}

}  // namespace digitqt::formats
