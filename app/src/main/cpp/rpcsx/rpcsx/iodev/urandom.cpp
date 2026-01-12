#include "orbis/IoDevice.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/uio.hpp"
#include <cstring>
#include <span>

struct UrandomDevice : public orbis::IoDevice {
  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};
struct UrandomFile : public orbis::File {};

static orbis::ErrorCode urandom_read(orbis::File *file, orbis::Uio *uio,
                                     orbis::Thread *) {
  for (auto entry : std::span(uio->iov, uio->iovcnt)) {
    std::memset(entry.base, 0, entry.len);
    uio->offset += entry.len;
  }
  uio->resid = 0;
  return {};
}

static const orbis::FileOps ops = {
    .read = urandom_read,
};

orbis::ErrorCode UrandomDevice::open(rx::Ref<orbis::File> *file,
                                     const char *path, std::uint32_t flags,
                                     std::uint32_t mode,
                                     orbis::Thread *thread) {
  auto newFile = orbis::knew<UrandomFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

orbis::IoDevice *createUrandomCharacterDevice() {
  return orbis::knew<UrandomDevice>();
}
