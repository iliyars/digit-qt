#pragma once

#include <optional>
#include <string_view>

namespace digitqt::formats::text {

inline constexpr std::string_view kWhitespace = " \t\r\n";

/// Значение строки вида "Key=значение" (ключ -- в начале строки), без
/// хвостовых пробелов и '\r'. nullopt, если такой строки нет. Годится и
/// для заголовков секций: lineValue(text, "[MATRIX]") отвечает на вопрос
/// "есть ли строка, начинающаяся с [MATRIX]".
std::optional<std::string_view> lineValue(std::string_view text, std::string_view key);

/// Число из всего токена целиком. std::from_chars, а не strtod/stod: не
/// зависит от локали (в русской локали strtod ждёт запятую вместо точки)
/// и не требует нуль-терминатора.
bool parseDouble(std::string_view token, double &out);

/// Выдаёт токены, разделённые пробельными символами, по одному -- без
/// списка всех токенов в памяти (в матрице .mtr 1023×1023 их около двух
/// миллионов).
class Tokenizer {
public:
  explicit Tokenizer(std::string_view text) : m_rest(text) {}

  bool next(std::string_view &token);

  /// Номер последнего выданного токена, с нуля -- для сообщений об ошибках.
  int index() const { return m_index - 1; }

private:
  std::string_view m_rest;
  int m_index = 0;
};

}  // namespace digitqt::formats::text
