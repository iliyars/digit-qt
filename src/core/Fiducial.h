#pragma once

#include <string>
#include <vector>

namespace digitqt::core {

/**
 * @brief Репер -- точка с известной позицией в СКИ (пиксели изображения,
 * где пользователь её поставил) и, опционально, известной позицией в
 * другой системе координат (например, физическая позиция на
 * калибровочной мишени, или будущая СКОС/СКД).
 *
 * Пока в приложении нет формальных G2/G3-преобразований, referenceX/Y
 * хранится как есть, без привязки к конкретной СК -- какая именно это
 * система, определяется контекстом использования (см. FiducialSet).
 * Когда появится подгонка трансформации по реперам, сюда же добавятся
 * weight/confidence -- сейчас их вводить рано, нечем ещё пользоваться.
 */
struct Fiducial {
  int id = 0;

  // Позиция в СКИ -- всегда известна, это то, что пользователь кликнул.
  double imageX = 0.0;
  double imageY = 0.0;

  // Вторая (эталонная) координата -- известна не всегда.
  bool hasReference = false;
  double referenceX = 0.0;
  double referenceY = 0.0;

  // Человекочитаемая метка (например, "1", "A", "центр") -- для
  // отображения в UI и сопоставления с внешним списком координат при
  // импорте эталонных значений. Не обязана быть уникальной сама по себе
  // -- уникальность гарантирует id.
  std::string label;
};

/**
 * @brief Живые, редактируемые реперы -- аналог FringeTracingData::seeds()
 */
class FiducialSet {
public:
  std::vector<Fiducial> &fiducials() { return m_fiducials; }
  const std::vector<Fiducial> &fiducials() const { return m_fiducials; }

  /// Следующий свободный id -- монотонно растёт, не переиспользуется
  /// после удаления (чтобы id оставался стабильным идентификатором репера
  /// на весь срок жизни документа, а не индексом в векторе).
  int nextId() const { return m_nextId; }
  int allocateId() { return m_nextId++; }

  void clear() {
    m_fiducials.clear();
    m_nextId = 0;
  }

private:
  std::vector<Fiducial> m_fiducials;
  int m_nextId = 0;
};

}  // namespace digitqt::core
