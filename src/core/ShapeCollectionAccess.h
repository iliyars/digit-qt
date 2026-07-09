#pragma once

#include <aperture/include/visibility/ShapeCollection.h>

namespace digitqt::core {
inline std::vector<std::unique_ptr<aperture::Shape>> &mutableContainer(
    aperture::ShapeCollection &collection, aperture::TypeLimits type) {
  switch (type) {
    case aperture::TypeLimits::EXTERNAL:
      return const_cast<std::vector<std::unique_ptr<aperture::Shape>> &>(
          collection.getExternal());
    case aperture::TypeLimits::INTERNAL:
      return const_cast<std::vector<std::unique_ptr<aperture::Shape>> &>(
          collection.getInternal());
    case aperture::TypeLimits::APERTURE:
      return const_cast<std::vector<std::unique_ptr<aperture::Shape>> &>(
          collection.getApertures());
  }
  throw std::logic_error("mutableContainer: unhandled TypeLimits value");
}
}  // namespace digitqt::core
