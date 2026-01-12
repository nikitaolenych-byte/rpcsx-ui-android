#include "orbis/IoDevice.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"

struct NpdrmFile : orbis::File {};

static orbis::ErrorCode npdrm_ioctl(orbis::File *file, std::uint64_t request,
                                    void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled NPDRM ioctl", request);
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = npdrm_ioctl,
};

struct NpdrmDevice : orbis::IoDevice {
  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<NpdrmFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

orbis::IoDevice *createNpdrmCharacterDevice() {
  return orbis::knew<NpdrmDevice>();
}
