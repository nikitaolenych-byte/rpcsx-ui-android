#pragma once

#include "orbis/error/SysResult.hpp"
#include "orbis/file.hpp"
#include "rx/Rc.hpp"
#include <filesystem>

namespace orbis {
struct IoDevice;
}

namespace vfs {
void fork();
void initialize();
void deinitialize();
void addDevice(std::string name, orbis::IoDevice *device);
std::pair<rx::Ref<orbis::IoDevice>, std::string>
get(const std::filesystem::path &guestPath);
orbis::SysResult mount(const std::filesystem::path &guestPath,
                       orbis::IoDevice *dev);
orbis::SysResult open(std::string_view path, int flags, int mode,
                      rx::Ref<orbis::File> *file, orbis::Thread *thread);
bool exists(std::string_view path, orbis::Thread *thread);
orbis::SysResult mkdir(std::string_view path, int mode, orbis::Thread *thread);
orbis::SysResult rmdir(std::string_view path, orbis::Thread *thread);
orbis::SysResult rename(std::string_view from, std::string_view to,
                        orbis::Thread *thread);
orbis::ErrorCode unlink(std::string_view path, orbis::Thread *thread);
orbis::ErrorCode unlinkAll(std::string_view path, orbis::Thread *thread);
orbis::ErrorCode createSymlink(std::string_view target,
                               std::string_view linkPath,
                               orbis::Thread *thread);
} // namespace vfs
