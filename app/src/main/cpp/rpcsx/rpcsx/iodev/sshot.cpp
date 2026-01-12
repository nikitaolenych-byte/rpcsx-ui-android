#include "orbis/IoDevice.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/thread/Thread.hpp"
#include "orbis/utils/Logs.hpp"

struct ScreenShotFile : orbis::File {};

static orbis::ErrorCode sshot_ioctl(orbis::File *file, std::uint64_t request,
                                    void *argp, orbis::Thread *thread) {

  ORBIS_LOG_FATAL("Unhandled sshot ioctl", request);
  thread->where();
  return {};
}

static const orbis::FileOps fileOps = {
    .ioctl = sshot_ioctl,
};

struct ScreenShotDevice : orbis::IoDevice {
  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override {
    auto newFile = orbis::knew<ScreenShotFile>();
    newFile->ops = &fileOps;
    newFile->device = this;

    *file = newFile;
    return {};
  }
};

orbis::IoDevice *createScreenShotCharacterDevice() {
  return orbis::knew<ScreenShotDevice>();
}
