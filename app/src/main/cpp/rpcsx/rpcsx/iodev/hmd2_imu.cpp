#include "orbis/IoDevice.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"

struct Hmd2ImuDevice : public orbis::IoDevice {
  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};
struct Hmd2ImuFile : public orbis::File {};

static orbis::ErrorCode hmd2_imu_ioctl(orbis::File *file, std::uint64_t request,
                                       void *argp, orbis::Thread *thread) {
  ORBIS_LOG_FATAL("Unhandled hmd2_imu ioctl", request);
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = hmd2_imu_ioctl,
};

orbis::ErrorCode Hmd2ImuDevice::open(rx::Ref<orbis::File> *file,
                                     const char *path, std::uint32_t flags,
                                     std::uint32_t mode,
                                     orbis::Thread *thread) {
  auto newFile = orbis::knew<Hmd2ImuFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

orbis::IoDevice *createHmd2ImuCharacterDevice() {
  return orbis::knew<Hmd2ImuDevice>();
}
