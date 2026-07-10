#pragma once

#include "core/NumberedFringeLine.h"

#include <vector>

namespace digitqt::core {

/**
 * @brief Нумерует линии слева направо (0, 1, 2, ...) по среднему X.
 *
 * Линии с orderIsManual == true не изменяются. Они учитываются при
 * расстановке порядковых позиций, но сохраняют пользовательское значение,
 * даже если это ломает монотонность. Сделано намеренно.
 */
void autoAssignFringeOrder(std::vector<NumberedFringeLine> &lines);
}  // namespace digitqt::core
