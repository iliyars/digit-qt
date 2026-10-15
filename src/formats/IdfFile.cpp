#include "IdfFile.h"

#include "TextParsing.h"

#include <charconv>

namespace digitqt::formats {

namespace {

constexpr std::string_view kHeader = "Interferograms descriptive file";

std::string_view trim(std::string_view line) {
  const size_t begin = line.find_first_not_of(text::kWhitespace);
  if (begin == std::string_view::npos)
    return {};
  const size_t end = line.find_last_not_of(text::kWhitespace);
  return line.substr(begin, end - begin + 1);
}

/// Непустые строки файла без пробелов по краям. Пустая строка после
/// заголовка в формате есть, но смысла не несёт.
std::vector<std::string_view> nonEmptyLines(std::string_view text) {
  std::vector<std::string_view> lines;
  while (!text.empty()) {
    const size_t end = text.find('\n');
    const std::string_view line = trim(text.substr(0, end));
    if (!line.empty())
      lines.push_back(line);
    if (end == std::string_view::npos)
      break;
    text.remove_prefix(end + 1);
  }
  return lines;
}

bool parsePositiveInt(std::string_view token, int &out) {
  const auto result = std::from_chars(token.data(), token.data() + token.size(), out);
  return result.ec == std::errc() && result.ptr == token.data() + token.size() && out > 0;
}

}  // namespace

bool probeIdf(std::string_view head) {
  return trim(head.substr(0, head.find('\n'))) == kHeader;
}

bool parseIdf(std::string_view text, IdfDescription &out, std::string &error) {
  const std::vector<std::string_view> lines = nonEmptyLines(text);
  if (lines.empty() || lines[0] != kHeader) {
    error = "Not an interferogram series file: the first line must be \"" +
            std::string(kHeader) + "\"";
    return false;
  }

  const char *const names[] = {"frame count", "frame width", "frame height"};
  int values[3] = {};
  for (int i = 0; i < 3; ++i) {
    if (lines.size() <= static_cast<size_t>(i + 1) ||
        !parsePositiveInt(lines[i + 1], values[i])) {
      error = std::string("Missing or invalid ") + names[i];
      return false;
    }
  }
  const int frameCount = values[0];

  if (lines.size() < static_cast<size_t>(4 + frameCount)) {
    error = "The file declares " + std::to_string(frameCount) + " frames but lists only " +
            std::to_string(lines.size() - 4);
    return false;
  }

  IdfDescription description;
  description.width = values[1];
  description.height = values[2];
  for (int i = 0; i < frameCount; ++i)
    description.frameFiles.emplace_back(lines[4 + i]);
  out = std::move(description);
  return true;
}

}  // namespace digitqt::formats
