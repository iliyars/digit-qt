#include "TextParsing.h"

#include <charconv>

namespace digitqt::formats::text {

std::optional<std::string_view> lineValue(std::string_view text, std::string_view key) {
  size_t pos = 0;
  while ((pos = text.find(key, pos)) != std::string_view::npos) {
    if (pos == 0 || text[pos - 1] == '\n') {
      const size_t start = pos + key.size();
      const size_t end = text.find('\n', start);
      std::string_view value = text.substr(start, end == std::string_view::npos ? end : end - start);
      const size_t last = value.find_last_not_of(kWhitespace);
      return last == std::string_view::npos ? std::string_view() : value.substr(0, last + 1);
    }
    pos += key.size();
  }
  return std::nullopt;
}

bool parseDouble(std::string_view token, double &out) {
  if (!token.empty() && token.front() == '+')
    token.remove_prefix(1);
  const char *end = token.data() + token.size();
  const auto result = std::from_chars(token.data(), end, out);
  return result.ec == std::errc() && result.ptr == end;
}

bool Tokenizer::next(std::string_view &token) {
  const size_t start = m_rest.find_first_not_of(kWhitespace);
  if (start == std::string_view::npos)
    return false;
  const size_t end = m_rest.find_first_of(kWhitespace, start);
  token = m_rest.substr(start, end == std::string_view::npos ? end : end - start);
  m_rest = end == std::string_view::npos ? std::string_view() : m_rest.substr(end);
  ++m_index;
  return true;
}

}  // namespace digitqt::formats::text
