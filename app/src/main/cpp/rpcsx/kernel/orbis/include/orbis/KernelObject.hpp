#pragma once
#include "rx/Serializer.hpp"
#include <cstddef>
#include <kernel/KernelObject.hpp>

namespace orbis {
struct OrbisNamespace;

extern std::byte *g_globalStorage;

template <rx::Serializable T> class GlobalObjectRef {
  std::uint32_t mOffset;

public:
  explicit GlobalObjectRef(std::uint32_t offset) : mOffset(offset) {}
  T *get() { return reinterpret_cast<T *>(g_globalStorage + mOffset); }
  T *operator->() { return get(); }
  T &operator*() { return *get(); }
};

template <rx::Serializable StateT>
GlobalObjectRef<StateT> createGlobalObject() {
  auto layoutOffset = kernel::StaticKernelObjectStorage<
      OrbisNamespace, kernel::detail::GlobalScope>::template Allocate<StateT>();

  return GlobalObjectRef<StateT>(layoutOffset);
}

template <rx::Serializable StateT>
kernel::StaticObjectRef<OrbisNamespace, kernel::detail::ProcessScope, StateT>
createProcessLocalObject() {
  auto layoutOffset = kernel::StaticKernelObjectStorage<
      OrbisNamespace,
      kernel::detail::ProcessScope>::template Allocate<StateT>();
  return {layoutOffset};
}

template <rx::Serializable StateT>
kernel::StaticObjectRef<OrbisNamespace, kernel::detail::ThreadScope, StateT>
createThreadLocalObject() {
  auto layoutOffset = kernel::StaticKernelObjectStorage<
      OrbisNamespace, kernel::detail::ThreadScope>::template Allocate<StateT>();
  return {layoutOffset};
}

inline void constructAllGlobals() {
  kernel::StaticKernelObjectStorage<
      OrbisNamespace,
      kernel::detail::GlobalScope>::ConstructAll(g_globalStorage);
}

inline void destructAllGlobals() {
  kernel::StaticKernelObjectStorage<
      OrbisNamespace,
      kernel::detail::GlobalScope>::DestructAll(g_globalStorage);
}

inline void serializeAllGlobals(rx::Serializer &s) {
  kernel::StaticKernelObjectStorage<
      OrbisNamespace,
      kernel::detail::GlobalScope>::SerializeAll(g_globalStorage, s);
}

inline void deserializeAllGlobals(rx::Deserializer &s) {
  kernel::StaticKernelObjectStorage<
      OrbisNamespace,
      kernel::detail::GlobalScope>::DeserializeAll(g_globalStorage, s);
}
} // namespace orbis
