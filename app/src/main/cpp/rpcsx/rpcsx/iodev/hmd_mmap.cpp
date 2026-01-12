#include "orbis/IoDevice.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"
#include "vm.hpp"

struct HmdMmapDevice : public orbis::IoDevice {
  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};
struct HmdMmapFile : public orbis::File {};

static orbis::ErrorCode hmd_mmap_ioctl(orbis::File *file, std::uint64_t request,
                                       void *argp, orbis::Thread *thread) {
  ORBIS_LOG_FATAL("Unhandled hmd_mmap ioctl", request);

  // 0x800c4802
  return {};
}

static orbis::ErrorCode hmd_mmap_mmap(orbis::File *file, void **address,
                                      std::uint64_t size, std::int32_t prot,
                                      std::int32_t flags, std::int64_t offset,
                                      orbis::Thread *thread) {
  ORBIS_LOG_FATAL("hmd_mmap mmap", address, size, offset);
  auto result = vm::map(*address, size, prot, flags);

  if (result == (void *)-1) {
    return orbis::ErrorCode::INVAL; // TODO
  }

  *address = result;
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = hmd_mmap_ioctl,
    .mmap = hmd_mmap_mmap,
};

orbis::ErrorCode HmdMmapDevice::open(rx::Ref<orbis::File> *file,
                                     const char *path, std::uint32_t flags,
                                     std::uint32_t mode,
                                     orbis::Thread *thread) {
  auto newFile = orbis::knew<HmdMmapFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

orbis::IoDevice *createHmdMmapCharacterDevice() {
  return orbis::knew<HmdMmapDevice>();
}
