#pragma once

#include "core/NumberedFringeLine.h"

#include <vector>

namespace digitqt::core {

/**
 * @brief Нумерует открытые линии по соседству в общих поперечных сечениях.
 * Для несвязных/замкнутых наборов сохраняется запасной порядок по среднему X.
 *
 * Линии с orderIsManual == true не изменяются. Они учитываются при
 * расстановке порядковых позиций, но сохраняют пользовательское значение,
 * даже если это ломает монотонность. Сделано намеренно.
 */
void autoAssignFringeOrder(std::vector<NumberedFringeLine> &lines);
}  // namespace digitqt::core
