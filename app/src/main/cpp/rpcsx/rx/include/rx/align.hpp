#pragma once

#include <type_traits>

namespace rx {
template <typename T, typename U>
  requires std::is_unsigned_v<T>
inline constexpr std::make_unsigned_t<std::common_type_t<T, U>>
alignUp(T value, U alignment) {
  return static_cast<std::make_unsigned_t<std::common_type_t<T, U>>>(
      (value + (alignment - 1)) & ~(alignment - 1));
}

template <typename T, typename U>
  requires std::is_unsigned_v<T>
inline constexpr std::make_unsigned_t<std::common_type_t<T, U>>
alignDown(T value, U alignment) {
  return static_cast<std::make_unsigned_t<std::common_type_t<T, U>>>(
      value & ~(alignment - 1));
}
} // namespace rx
