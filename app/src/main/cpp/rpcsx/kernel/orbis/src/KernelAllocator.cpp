#include "KernelAllocator.hpp"
#include "KernelObject.hpp"
#include "rx/Serializer.hpp"
#include "rx/SharedMutex.hpp"
#include "rx/print.hpp"
#include <sys/mman.h>

static const std::uint64_t g_allocProtWord = 0xDEADBEAFBADCAFE1;
static constexpr std::uintptr_t kHeapBaseAddress = 0x00000600'0000'0000;
static constexpr auto kHeapSize = 0x1'0000'0000;
static constexpr int kDebugHeap = 0;

namespace orbis {
struct KernelMemoryResource {
  mutable rx::shared_mutex m_heap_mtx;
  rx::shared_mutex m_heap_map_mtx;
  void *m_heap_next = nullptr;

  kmultimap<std::size_t, void *> m_free_heap;
  kmultimap<std::size_t, void *> m_used_node;

  ~KernelMemoryResource() {
    ::munmap(std::bit_cast<void *>(kHeapBaseAddress), kHeapSize);
  }

  void *kalloc(std::size_t size,
               std::size_t align = __STDCPP_DEFAULT_NEW_ALIGNMENT__);
  void kfree(void *ptr, std::size_t size);

  void serialize(rx::Serializer &) const {
    // FIXME: implement
  }
  void deserialize(rx::Deserializer &) {
    // FIXME: implement
  }

  void lock() const { m_heap_mtx.lock(); }
  void unlock() const { m_heap_mtx.unlock(); }
};

static KernelMemoryResource *sMemoryResource;
std::byte *g_globalStorage;

using GlobalStorage =
    kernel::StaticKernelObjectStorage<OrbisNamespace,
                                      kernel::detail::GlobalScope>;

void initializeAllocator() {
  auto ptr = (std::byte *)::mmap(std::bit_cast<void *>(kHeapBaseAddress),
                                 kHeapSize, PROT_READ | PROT_WRITE,
                                 MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

  if (ptr == MAP_FAILED) {
    perror("mmap failed");
    FILE *maps = std::fopen("/proc/self/maps", "r");
    char *line = nullptr;
    std::size_t size = 0;
    while (getline(&line, &size, maps) > 0) {
      std::puts(line);
    }

    std::free(line);
    std::fclose(maps);
    std::abort();
  }

  sMemoryResource = new (ptr) KernelMemoryResource();
  sMemoryResource->m_heap_next = ptr + sizeof(KernelMemoryResource);

  rx::print(stderr, "global: size {}, alignment {}\n", GlobalStorage::GetSize(),
            GlobalStorage::GetAlignment());
  // allocate whole global storage
  g_globalStorage = (std::byte *)sMemoryResource->kalloc(
      GlobalStorage::GetSize(), GlobalStorage::GetAlignment());
}

void deinitializeAllocator() {
  sMemoryResource->kfree(g_globalStorage, GlobalStorage::GetSize());
  delete sMemoryResource;
  sMemoryResource = nullptr;
  g_globalStorage = nullptr;
}

void *KernelMemoryResource::kalloc(std::size_t size, std::size_t align) {
  size = (size + (__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1)) &
         ~(__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1);
  if (!size)
    std::abort();

  if (m_heap_map_mtx.try_lock()) {
    std::lock_guard lock(m_heap_map_mtx, std::adopt_lock);

    // Try to reuse previously freed block
    for (auto [it, end] = m_free_heap.equal_range(size); it != end; ++it) {
      auto result = it->second;
      if (!(std::bit_cast<std::uintptr_t>(result) & (align - 1))) {
        auto node = m_free_heap.extract(it);
        node.key() = 0;
        node.mapped() = nullptr;
        m_used_node.insert(m_used_node.begin(), std::move(node));

        // std::fprintf(stderr, "kalloc: reuse %p-%p, size = %lx\n", result,
        //              (char *)result + size, size);

        if (kDebugHeap > 0) {
          std::memcpy(std::bit_cast<std::byte *>(result) + size,
                      &g_allocProtWord, sizeof(g_allocProtWord));
        }
        return result;
      }
    }
  }

  std::lock_guard lock(m_heap_mtx);
  align = std::max<std::size_t>(align, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
  auto heap = reinterpret_cast<std::uintptr_t>(m_heap_next);
  heap = (heap + (align - 1)) & ~(align - 1);

  if (kDebugHeap > 1) {
    if (auto diff = (heap + size + sizeof(g_allocProtWord)) % 4096; diff != 0) {
      heap += 4096 - diff;
      heap &= ~(align - 1);
    }
  }

  if (heap + size > kHeapBaseAddress + kHeapSize) {
    std::fprintf(stderr, "out of kernel memory");
    std::abort();
  }
  // Check overflow
  if (heap + size < heap) {
    std::fprintf(stderr, "too big allocation");
    std::abort();
  }

  // std::fprintf(stderr, "kalloc: allocate %lx-%lx, size = %lx, align=%lx\n",
  //              heap, heap + size, size, align);

  auto result = reinterpret_cast<void *>(heap);
  if (kDebugHeap > 0) {
    std::memcpy(std::bit_cast<std::byte *>(result) + size, &g_allocProtWord,
                sizeof(g_allocProtWord));
  }

  if (kDebugHeap > 0) {
    m_heap_next =
        reinterpret_cast<void *>(heap + size + sizeof(g_allocProtWord));
  } else {
    m_heap_next = reinterpret_cast<void *>(heap + size);
  }

  if (kDebugHeap > 1) {
    heap = reinterpret_cast<std::uintptr_t>(m_heap_next);
    align = std::min<std::size_t>(align, 4096);
    heap = (heap + (align - 1)) & ~(align - 1);
    size = 4096;
    // std::fprintf(stderr, "kalloc: protect %lx-%lx, size = %lx, align=%lx\n",
    //              heap, heap + size, size, align);

    auto result = ::mmap(reinterpret_cast<void *>(heap), size, PROT_NONE,
                         MAP_FIXED | MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (result == MAP_FAILED) {
      std::fprintf(stderr, "failed to protect memory");
      std::abort();
    }
    m_heap_next = reinterpret_cast<void *>(heap + size);
  }

  return result;
}

void KernelMemoryResource::kfree(void *ptr, std::size_t size) {
  size = (size + (__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1)) &
         ~(__STDCPP_DEFAULT_NEW_ALIGNMENT__ - 1);
  if (!size)
    std::abort();

  if (std::bit_cast<std::uintptr_t>(ptr) < kHeapBaseAddress ||
      std::bit_cast<std::uintptr_t>(ptr) + size >
          kHeapBaseAddress + kHeapSize) {
    std::fprintf(stderr, "kfree: invalid address");
    std::abort();
  }

  if (kDebugHeap > 0) {
    if (std::memcmp(std::bit_cast<std::byte *>(ptr) + size, &g_allocProtWord,
                    sizeof(g_allocProtWord)) != 0) {
      std::fprintf(stderr, "kernel heap corruption\n");
      std::abort();
    }

    std::memset(ptr, 0xcc, size + sizeof(g_allocProtWord));
  }

  // std::fprintf(stderr, "kfree: release %p-%p, size = %lx\n", ptr,
  //              (char *)ptr + size, size);

  std::lock_guard lock(m_heap_map_mtx);
  if (!m_used_node.empty()) {
    auto node = m_used_node.extract(m_used_node.begin());
    node.key() = size;
    node.mapped() = ptr;
    m_free_heap.insert(std::move(node));
  } else {
    m_free_heap.emplace(size, ptr);
  }
}

void kfree(void *ptr, std::size_t size) {
  return sMemoryResource->kfree(ptr, size);
}

void *kalloc(std::size_t size, std::size_t align) {
  return sMemoryResource->kalloc(size, align);
}
} // namespace orbis
