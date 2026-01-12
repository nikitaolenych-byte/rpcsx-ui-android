#include "orbis/IoDevice.hpp"
#include "orbis/KernelAllocator.hpp"
#include "orbis/file.hpp"
#include "orbis/utils/Logs.hpp"

struct HmdSnsrDevice : public orbis::IoDevice {
  orbis::ErrorCode open(rx::Ref<orbis::File> *file, const char *path,
                        std::uint32_t flags, std::uint32_t mode,
                        orbis::Thread *thread) override;
};
struct HmdSnsrFile : public orbis::File {};

static orbis::ErrorCode hmd_snsr_ioctl(orbis::File *file, std::uint64_t request,
                                       void *argp, orbis::Thread *thread) {
  ORBIS_LOG_FATAL("Unhandled hmd_snsr ioctl", request);

  // 0x800c4802
  return {};
}

static const orbis::FileOps ops = {
    .ioctl = hmd_snsr_ioctl,
};

orbis::ErrorCode HmdSnsrDevice::open(rx::Ref<orbis::File> *file,
                                     const char *path, std::uint32_t flags,
                                     std::uint32_t mode,
                                     orbis::Thread *thread) {
  auto newFile = orbis::knew<HmdSnsrFile>();
  newFile->device = this;
  newFile->ops = &ops;
  *file = newFile;
  return {};
}

orbis::IoDevice *createHmdSnsrCharacterDevice() {
  return orbis::knew<HmdSnsrDevice>();
}
