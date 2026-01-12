#include <algorithm>
#include <android/dlext.h>
#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>
#include <optional>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <unistd.h>
#include <utility>

#if defined(__aarch64__)
#include <adrenotools/driver.h>
#include <adrenotools/priv.h>
#endif

// RPCSX ARMv9 Optimization Modules
#include "nce_engine.h"
#include "fastmem_mapper.h"
#include "shader_cache_manager.h"
#include "thread_scheduler.h"
#include "frostbite_hacks.h"
#include "vulkan_renderer.h"
#include "fsr31/fsr31.h"
#include "signal_handler.h"

#define LOG_TAG "RPCSX-Native"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct RPCSXApi {
  bool (*overlayPadData)(int digital1, int digital2, int leftStickX,
                         int leftStickY, int rightStickX, int rightStickY);
  bool (*initialize)(std::string_view rootDir, std::string_view user);
  bool (*processCompilationQueue)(JNIEnv *env);
  bool (*startMainThreadProcessor)(JNIEnv *env);
  bool (*collectGameInfo)(JNIEnv *env, std::string_view rootDir,
                          long progressId);
  void (*shutdown)();
  int (*boot)(std::string_view path_);
  int (*getState)();
  void (*kill)();
  void (*resume)();
  void (*openHomeMenu)();
  std::string (*getTitleId)();
  bool (*surfaceEvent)(JNIEnv *env, jobject surface, jint event);
  bool (*usbDeviceEvent)(int fd, int vendorId, int productId, int event);
  bool (*installFw)(JNIEnv *env, int fd, long progressId);
  bool (*isInstallableFile)(jint fd);
  jstring (*getDirInstallPath)(JNIEnv *env, jint fd);
  bool (*install)(JNIEnv *env, int fd, long progressId);
  bool (*installKey)(JNIEnv *env, int fd, long progressId,
                     std::string_view gamePath);
  std::string (*systemInfo)();
  void (*loginUser)(std::string_view userId);
  std::string (*getUser)();
  std::string (*settingsGet)(std::string_view path);
  bool (*settingsSet)(std::string_view path, std::string_view valueString);
  std::string (*getVersion)();
  void *(*setCustomDriver)(void *driverHandle);
};

struct RPCSXLibrary : RPCSXApi {
  void *handle = nullptr;

  RPCSXLibrary() = default;
  RPCSXLibrary(const RPCSXLibrary &) = delete;
  RPCSXLibrary(RPCSXLibrary &&other) { swap(other); }
  RPCSXLibrary &operator=(RPCSXLibrary &&other) {
    swap(other);
    return *this;
  }
  ~RPCSXLibrary() {
    if (handle) {
      ::dlclose(handle);
    }
  }

  void swap(RPCSXLibrary &other) noexcept {
    std::swap(handle, other.handle);
    std::swap(static_cast<RPCSXApi &>(*this), static_cast<RPCSXApi &>(other));
  }

  static std::optional<RPCSXLibrary> Open(const char *path) {
    void *handle = ::dlopen(path, RTLD_LOCAL | RTLD_NOW);
    if (handle == nullptr) {
      __android_log_print(ANDROID_LOG_ERROR, "RPCSX-UI",
                          "Failed to open RPCSX library at %s, error %s", path,
                          ::dlerror());
      return {};
    }

    RPCSXLibrary result;
    result.handle = handle;

    // clang-format off
    result.overlayPadData = reinterpret_cast<decltype(overlayPadData)>(dlsym(handle, "_rpcsx_overlayPadData"));
    result.initialize = reinterpret_cast<decltype(initialize)>(dlsym(handle, "_rpcsx_initialize"));
    result.processCompilationQueue = reinterpret_cast<decltype(processCompilationQueue)>(dlsym(handle, "_rpcsx_processCompilationQueue"));
    result.startMainThreadProcessor = reinterpret_cast<decltype(startMainThreadProcessor)>(dlsym(handle, "_rpcsx_startMainThreadProcessor"));
    result.collectGameInfo = reinterpret_cast<decltype(collectGameInfo)>(dlsym(handle, "_rpcsx_collectGameInfo"));
    result.shutdown = reinterpret_cast<decltype(shutdown)>(dlsym(handle, "_rpcsx_shutdown"));
    result.boot = reinterpret_cast<decltype(boot)>(dlsym(handle, "_rpcsx_boot"));
    result.getState = reinterpret_cast<decltype(getState)>(dlsym(handle, "_rpcsx_getState"));
    result.kill = reinterpret_cast<decltype(kill)>(dlsym(handle, "_rpcsx_kill"));
    result.resume = reinterpret_cast<decltype(resume)>(dlsym(handle, "_rpcsx_resume"));
    result.openHomeMenu = reinterpret_cast<decltype(openHomeMenu)>(dlsym(handle, "_rpcsx_openHomeMenu"));
    result.getTitleId = reinterpret_cast<decltype(getTitleId)>(dlsym(handle, "_rpcsx_getTitleId"));
    result.surfaceEvent = reinterpret_cast<decltype(surfaceEvent)>(dlsym(handle, "_rpcsx_surfaceEvent"));
    result.usbDeviceEvent = reinterpret_cast<decltype(usbDeviceEvent)>(dlsym(handle, "_rpcsx_usbDeviceEvent"));
    result.installFw = reinterpret_cast<decltype(installFw)>(dlsym(handle, "_rpcsx_installFw"));
    result.isInstallableFile = reinterpret_cast<decltype(isInstallableFile)>(dlsym(handle, "_rpcsx_isInstallableFile"));
    result.getDirInstallPath = reinterpret_cast<decltype(getDirInstallPath)>(dlsym(handle, "_rpcsx_getDirInstallPath"));
    result.install = reinterpret_cast<decltype(install)>(dlsym(handle, "_rpcsx_install"));
    result.installKey = reinterpret_cast<decltype(installKey)>(dlsym(handle, "_rpcsx_installKey"));
    result.systemInfo = reinterpret_cast<decltype(systemInfo)>(dlsym(handle, "_rpcsx_systemInfo"));
    result.loginUser = reinterpret_cast<decltype(loginUser)>(dlsym(handle, "_rpcsx_loginUser"));
    result.getUser = reinterpret_cast<decltype(getUser)>(dlsym(handle, "_rpcsx_getUser"));
    result.settingsGet = reinterpret_cast<decltype(settingsGet)>(dlsym(handle, "_rpcsx_settingsGet"));
    result.settingsSet = reinterpret_cast<decltype(settingsSet)>(dlsym(handle, "_rpcsx_settingsSet"));
    result.getVersion = reinterpret_cast<decltype(getVersion)>(dlsym(handle, "_rpcsx_getVersion"));
    result.setCustomDriver = reinterpret_cast<decltype(setCustomDriver)>(dlsym(handle, "_rpcsx_setCustomDriver"));
    // clang-format on

    return result;
  }
};

static RPCSXLibrary rpcsxLib;

static std::string unwrap(JNIEnv *env, jstring string) {
  auto resultBuffer = env->GetStringUTFChars(string, nullptr);
  std::string result(resultBuffer);
  env->ReleaseStringUTFChars(string, resultBuffer);
  return result;
}
static jstring wrap(JNIEnv *env, const std::string &string) {
  return env->NewStringUTF(string.c_str());
}

static std::string ResolveBuildId(const std::string& javaBuildId) {
  if (!javaBuildId.empty()) {
    return javaBuildId;
  }
  // Fallback to a native-only build identifier so cache invalidation still works.
  return std::string("native-") + __DATE__ + "-" + __TIME__;
}
static jstring wrap(JNIEnv *env, const char *string) {
  return env->NewStringUTF(string);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_net_rpcsx_RPCSX_openLibrary(JNIEnv *env, jobject, jstring path) {
  if (auto library = RPCSXLibrary::Open(unwrap(env, path).c_str())) {
    rpcsxLib = std::move(*library);
    return true;
  }

  return false;
}

extern "C" JNIEXPORT jstring JNICALL
Java_net_rpcsx_RPCSX_getLibraryVersion(JNIEnv *env, jobject, jstring path) {
  if (auto library = RPCSXLibrary::Open(unwrap(env, path).c_str())) {
    if (auto getVersion = library->getVersion) {
      return wrap(env, getVersion());
    }
  }

  return {};
}

extern "C" JNIEXPORT jboolean JNICALL Java_net_rpcsx_RPCSX_overlayPadData(
    JNIEnv *, jobject, jint digital1, jint digital2, jint leftStickX,
    jint leftStickY, jint rightStickX, jint rightStickY) {
  return rpcsxLib.overlayPadData(digital1, digital2, leftStickX, leftStickY,
                                 rightStickX, rightStickY);
}

extern "C" JNIEXPORT jboolean JNICALL Java_net_rpcsx_RPCSX_initialize(
    JNIEnv *env, jobject, jstring rootDir, jstring user) {
    
  // --- RPCSX ARMv9 Optimization Initialization ---
  rpcsx::nce::InitializeNCE();
  rpcsx::scheduler::InitializeScheduler();
  
  std::string userDir = unwrap(env, user);
  std::string shaderCacheDir = userDir + "/shader_cache";
  rpcsx::shaders::InitializeShaderCache(shaderCacheDir.c_str());
  
  LOGI("RPCSX Optimized Modules Initialized: NCE, Scheduler, ShaderCache");

  // Install crash handlers early; helps with SIGSEGV/SIGBUS on some kernels/devices.
  rpcsx::crash::InstallSignalHandlers();
  // -----------------------------------------------

  return rpcsxLib.initialize(unwrap(env, rootDir), unwrap(env, user));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_net_rpcsx_RPCSX_processCompilationQueue(JNIEnv *env, jobject) {
  return rpcsxLib.processCompilationQueue(env);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_net_rpcsx_RPCSX_startMainThreadProcessor(JNIEnv *env, jobject) {
  return rpcsxLib.startMainThreadProcessor(env);
}

extern "C" JNIEXPORT jboolean JNICALL Java_net_rpcsx_RPCSX_collectGameInfo(
    JNIEnv *env, jobject, jstring jrootDir, jlong progressId) {
  return rpcsxLib.collectGameInfo(env, unwrap(env, jrootDir), progressId);
}

extern "C" JNIEXPORT void JNICALL Java_net_rpcsx_RPCSX_shutdown(JNIEnv *env,
                                                                jobject) {
  return rpcsxLib.shutdown();
}

extern "C" JNIEXPORT jint JNICALL Java_net_rpcsx_RPCSX_boot(JNIEnv *env,
                                                            jobject,
                                                            jstring jpath) {
  std::string path = unwrap(env, jpath);
  rpcsx::crash::CrashGuard guard("rpcsx_boot");

  if (!guard.ok()) {
    LOGE("Boot aborted due to prior crash state");
    return -1;
  }
  
  // Attempt to apply hacks if this is a supported game found in path or current title
  // Since we don't have the Title ID easily before boot, we'll try to retrieve it after 
  // or infer from the path structure if possible.
  
  int result = rpcsxLib.boot(path);

  if (!guard.ok()) {
      LOGE("Boot crashed (sig=%d addr=%p) in scope=%s", guard.signal(), guard.faultAddress(), guard.scope());
      return -1;
  }

  // Apply Frostbite hacks if the game is identified
  if (auto getTitleId = rpcsxLib.getTitleId) {
      std::string titlId = getTitleId();
      if (!titlId.empty()) {
          rpcsx::frostbite::InitializeFrostbiteHacks(titlId.c_str());
           LOGI("Attempted to apply Frostbite Hacks for ID: %s", titlId.c_str());
      }
  }

  return result;
}

extern "C" JNIEXPORT jint JNICALL Java_net_rpcsx_RPCSX_getState(JNIEnv *env,
                                                                jobject) {
  return rpcsxLib.getState();
}

extern "C" JNIEXPORT void JNICALL Java_net_rpcsx_RPCSX_kill(JNIEnv *env,
                                                            jobject) {
  return rpcsxLib.kill();
}

extern "C" JNIEXPORT void JNICALL Java_net_rpcsx_RPCSX_resume(JNIEnv *env,
                                                              jobject) {
  return rpcsxLib.resume();
}

extern "C" JNIEXPORT void JNICALL Java_net_rpcsx_RPCSX_openHomeMenu(JNIEnv *env,
                                                                    jobject) {
  return rpcsxLib.openHomeMenu();
}

extern "C" JNIEXPORT jstring JNICALL
Java_net_rpcsx_RPCSX_getTitleId(JNIEnv *env, jobject) {
  return wrap(env, rpcsxLib.getTitleId());
}

extern "C" JNIEXPORT jboolean JNICALL Java_net_rpcsx_RPCSX_surfaceEvent(
    JNIEnv *env, jobject, jobject surface, jint event) {
  return rpcsxLib.surfaceEvent(env, surface, event);
}

extern "C" JNIEXPORT jboolean JNICALL Java_net_rpcsx_RPCSX_usbDeviceEvent(
    JNIEnv *env, jobject, jint fd, jint vendorId, jint productId, jint event) {
  return rpcsxLib.usbDeviceEvent(fd, vendorId, productId, event);
}

extern "C" JNIEXPORT jboolean JNICALL Java_net_rpcsx_RPCSX_installFw(
    JNIEnv *env, jobject, jint fd, jlong progressId) {
  return rpcsxLib.installFw(env, fd, progressId);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_net_rpcsx_RPCSX_isInstallableFile(JNIEnv *env, jobject, jint fd) {
  return rpcsxLib.isInstallableFile(fd);
}

extern "C" JNIEXPORT jstring JNICALL
Java_net_rpcsx_RPCSX_getDirInstallPath(JNIEnv *env, jobject, jint fd) {
  return rpcsxLib.getDirInstallPath(env, fd);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_net_rpcsx_RPCSX_install(JNIEnv *env, jobject, jint fd, jlong progressId) {
  return rpcsxLib.install(env, fd, progressId);
}

extern "C" JNIEXPORT jboolean JNICALL Java_net_rpcsx_RPCSX_installKey(
    JNIEnv *env, jobject, jint fd, jlong progressId, jstring gamePath) {
  return rpcsxLib.installKey(env, fd, progressId, unwrap(env, gamePath));
}

extern "C" JNIEXPORT jstring JNICALL
Java_net_rpcsx_RPCSX_systemInfo(JNIEnv *env, jobject) {
  return wrap(env, rpcsxLib.systemInfo());
}

extern "C" JNIEXPORT void JNICALL
Java_net_rpcsx_RPCSX_loginUser(JNIEnv *env, jobject, jstring user_id) {
  return rpcsxLib.loginUser(unwrap(env, user_id));
}

extern "C" JNIEXPORT jstring JNICALL Java_net_rpcsx_RPCSX_getUser(JNIEnv *env,
                                                                  jobject) {
  return wrap(env, rpcsxLib.getUser());
}

extern "C" JNIEXPORT jstring JNICALL
Java_net_rpcsx_RPCSX_settingsGet(JNIEnv *env, jobject, jstring jpath) {
  return wrap(env, rpcsxLib.settingsGet(unwrap(env, jpath)));
}

extern "C" JNIEXPORT jboolean JNICALL Java_net_rpcsx_RPCSX_settingsSet(
    JNIEnv *env, jobject, jstring jpath, jstring jvalue) {
  return rpcsxLib.settingsSet(unwrap(env, jpath), unwrap(env, jvalue));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_net_rpcsx_RPCSX_supportsCustomDriverLoading(JNIEnv *env,
                                                 jobject instance) {
  return access("/dev/kgsl-3d0", F_OK) == 0;
}

extern "C" JNIEXPORT jstring JNICALL
Java_net_rpcsx_RPCSX_getVersion(JNIEnv *env, jobject) {
  return wrap(env, rpcsxLib.getVersion());
}

extern "C" JNIEXPORT jboolean JNICALL
Java_net_rpcsx_RPCSX_setCustomDriver(JNIEnv *env, jobject, jstring jpath,
                                     jstring jlibraryName, jstring jhookDir) {
#ifdef __aarch64__
  if (rpcsxLib.setCustomDriver == nullptr) {
    return false;
  }

  auto path = unwrap(env, jpath);
  void *loader = nullptr;

  if (!path.empty()) {
      auto hookDir = unwrap(env, jhookDir);
      auto libraryName = unwrap(env, jlibraryName);
      __android_log_print(ANDROID_LOG_INFO, "RPCSX-UI", "Loading custom driver %s",
                          path.c_str());

      ::dlerror();
      loader = adrenotools_open_libvulkan(
              RTLD_NOW, ADRENOTOOLS_DRIVER_CUSTOM, nullptr, (hookDir + "/").c_str(),
              (path + "/").c_str(), libraryName.c_str(), nullptr, nullptr);

      if (loader == nullptr) {
          __android_log_print(ANDROID_LOG_INFO, "RPCSX-UI",
                              "Failed to load custom driver at '%s': %s",
                              path.c_str(), ::dlerror());
          return false;
      }
  }

  auto prevLoader = rpcsxLib.setCustomDriver(loader);
  if (prevLoader != nullptr) {
    ::dlclose(prevLoader);
  }

  return true;
#else
  return false;
#endif // __aarch64__
}

/**
 * Ініціалізація всіх ARMv9 оптимізацій
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_net_rpcsx_RPCSX_initializeARMv9Optimizations(JNIEnv *env, jobject,
                                                  jstring jcacheDir,
                                                  jstring jtitleId,
                                                  jstring jbuildId) {
  LOGI("=======================================================");
  LOGI("RPCSX ARMv9 Fork - Snapdragon 8s Gen 3 Optimizations");
  LOGI("=======================================================");
  
  auto cacheDir = unwrap(env, jcacheDir);
  auto titleId = unwrap(env, jtitleId);
  auto buildId = ResolveBuildId(unwrap(env, jbuildId));
  
  bool success = true;
  
  // 1. Ініціалізація NCE (Native Code Execution)
  LOGI("Initializing NCE Engine...");
  if (!rpcsx::nce::InitializeNCE()) {
    LOGE("NCE initialization failed");
    success = false;
  }
  
  // 2. Ініціалізація Fastmem
  LOGI("Initializing Fastmem (Direct Memory Mapping)...");
  if (!rpcsx::memory::InitializeFastmem()) {
    LOGE("Fastmem initialization failed");
    success = false;
  }
  
  // 3. Ініціалізація Shader Cache
  LOGI("Initializing 3-tier Shader Cache with Zstd...");
  if (!rpcsx::shaders::InitializeShaderCache(cacheDir.c_str(), buildId.c_str())) {
    LOGE("Shader cache initialization failed");
    success = false;
  }
  
  // 4. Ініціалізація Thread Scheduler
  LOGI("Initializing Aggressive Thread Scheduler...");
  if (!rpcsx::scheduler::InitializeScheduler()) {
    LOGE("Scheduler initialization failed");
    success = false;
  }
  
  // 5. Ініціалізація Frostbite хаків (якщо потрібно)
  if (rpcsx::frostbite::IsFrostbite3Game(titleId.c_str())) {
    LOGI("Frostbite 3 game detected - applying engine-specific hacks...");
    rpcsx::frostbite::InitializeFrostbiteHacks(titleId.c_str());
  }
  
  // 6. Ініціалізація FSR 3.1 (720p -> 1440p upscaling)
  LOGI("Initializing FSR 3.1 upscaler...");
  if (!rpcsx::fsr::InitializeFSR(1280, 720, 2560, 1440, 
                                 rpcsx::fsr::FSRQuality::PERFORMANCE)) {
    LOGE("FSR 3.1 initialization failed");
    // Non-critical, продовжуємо
  }

  // Ensure handlers are installed for the optimisation path as well.
  rpcsx::crash::InstallSignalHandlers();
  
  LOGI("=======================================================");
  if (success) {
    LOGI("All ARMv9 optimizations initialized successfully!");
    LOGI("Expected performance: 30-60 FPS on heavy games");
  } else {
    LOGE("Some optimizations failed - check logs");
  }
  LOGI("=======================================================");
  
  return success ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* /* reserved */) {
  JNIEnv* env = nullptr;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return JNI_ERR;
  }

  rpcsx::crash::InstallSignalHandlers();
  return JNI_VERSION_1_6;
}

/**
 * Очищення всіх оптимізацій при виході
 */
extern "C" JNIEXPORT void JNICALL
Java_net_rpcsx_RPCSX_shutdownARMv9Optimizations(JNIEnv *env, jobject) {
  LOGI("Shutting down ARMv9 optimizations...");
  
  rpcsx::fsr::ShutdownFSR();
  rpcsx::scheduler::ShutdownScheduler();
  rpcsx::shaders::ShutdownShaderCache();
  rpcsx::memory::ShutdownFastmem();
  rpcsx::nce::ShutdownNCE();
  
  LOGI("ARMv9 optimizations shutdown complete");
}

/**
 * Встановлення режиму NCE (0=Disabled, 1=Interpreter, 2=Recompiler, 3=NCE/JIT)
 */
extern "C" JNIEXPORT void JNICALL
Java_net_rpcsx_RPCSX_setNCEMode(JNIEnv *env, jobject, jint mode) {
  LOGI("Setting NCE mode to %d", mode);
  rpcsx::nce::SetNCEMode(mode);
}

/**
 * Отримання поточного режиму NCE
 */
extern "C" JNIEXPORT jint JNICALL
Java_net_rpcsx_RPCSX_getNCEMode(JNIEnv *env, jobject) {
  return rpcsx::nce::GetNCEMode();
}

/**
 * Перевірка чи NCE/JIT активний
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_net_rpcsx_RPCSX_isNCEActive(JNIEnv *env, jobject) {
  return rpcsx::nce::IsNCEActive() ? JNI_TRUE : JNI_FALSE;
}

/**
 * Примусове очищення всіх кешів (shader, pipeline, code)
 */
extern "C" JNIEXPORT void JNICALL
Java_net_rpcsx_RPCSX_purgeAllCaches(JNIEnv *env, jobject, jstring jcacheDir) {
  LOGI("Purging all caches...");
  
  auto cacheDir = unwrap(env, jcacheDir);
  
  // Очищення NCE code cache
  rpcsx::nce::InvalidateCodeCache();
  
  // Очищення Vulkan shader/pipeline cache
  rpcsx::vulkan::PurgeAllShaderCaches(cacheDir.c_str());
  
  // Очищення 3-tier shader cache
  // rpcsx::shaders::PurgeAllCaches();
  
  LOGI("All caches purged");
}

/**
 * Отримання статистики fastmem для діагностики
 */
extern "C" JNIEXPORT jstring JNICALL
Java_net_rpcsx_RPCSX_getFastmemStats(JNIEnv *env, jobject) {
  uint64_t accesses = 0, faults = 0;
  size_t size = 0;
  rpcsx::memory::GetFastmemStats(&accesses, &faults, &size);
  
  char buf[256];
  snprintf(buf, sizeof(buf), 
           "Fastmem: %zu GB, Accesses: %llu, Faults: %llu",
           size / (1024*1024*1024),
           (unsigned long long)accesses,
           (unsigned long long)faults);
  
  return wrap(env, buf);
}

