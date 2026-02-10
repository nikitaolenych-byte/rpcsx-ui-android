package net.rpcsx

import android.view.Surface
import androidx.compose.runtime.mutableStateOf

enum class Digital1Flags(val bit: Int)
{
    None(0),
    CELL_PAD_CTRL_SELECT(0x00000001),
    CELL_PAD_CTRL_L3(0x00000002),
    CELL_PAD_CTRL_R3(0x00000004),
    CELL_PAD_CTRL_START(0x00000008),
    CELL_PAD_CTRL_UP(0x00000010),
    CELL_PAD_CTRL_RIGHT(0x00000020),
    CELL_PAD_CTRL_DOWN(0x00000040),
    CELL_PAD_CTRL_LEFT(0x00000080),
    CELL_PAD_CTRL_PS(0x00000100),
}

enum class Digital2Flags(val bit: Int)
{
    None(0),
    CELL_PAD_CTRL_L2(0x00000001),
    CELL_PAD_CTRL_R2(0x00000002),
    CELL_PAD_CTRL_L1(0x00000004),
    CELL_PAD_CTRL_R1(0x00000008),
    CELL_PAD_CTRL_TRIANGLE(0x00000010),
    CELL_PAD_CTRL_CIRCLE(0x00000020),
    CELL_PAD_CTRL_CROSS(0x00000040),
    CELL_PAD_CTRL_SQUARE(0x00000080),
};

enum class EmulatorState {
    Stopped,
    Loading,
    Stopping,
    Running,
    Paused,
    Frozen, // paused but cannot resume
    Ready,
    Starting;

    companion object {
        fun fromInt(value: Int) = EmulatorState.entries.first { it.ordinal == value }
    }
}

enum class BootResult
{
    NoErrors,
    GenericError,
    NothingToBoot,
    WrongDiscLocation,
    InvalidFileOrFolder,
    InvalidBDvdFolder,
    InstallFailed,
    DecryptionError,
    FileCreationError,
    FirmwareMissing,
    UnsupportedDiscType,
    SavestateCorrupted,
    SavestateVersionUnsupported,
    StillRunning,
    AlreadyAdded,
    CurrentlyRestricted;

    companion object {
        fun fromInt(value: Int) = entries.first { it.ordinal == value }
    }
};

class RPCSX {
    external fun openLibrary(path: String): Boolean
    external fun getLibraryVersion(path: String): String?
    external fun initialize(rootDir: String, user: String): Boolean
    external fun installFw(fd: Int, progressId: Long): Boolean
    external fun install(fd: Int, progressId: Long): Boolean
    external fun installKey(fd: Int, requestId: Long, gamePath: String): Boolean
    external fun boot(path: String): Int
    external fun surfaceEvent(surface: Surface, event: Int): Boolean
    external fun usbDeviceEvent(fd: Int, vendorId: Int, productId: Int, event: Int): Boolean
    external fun processCompilationQueue(): Boolean
    external fun startMainThreadProcessor(): Boolean
    external fun overlayPadData(digital1: Int, digital2: Int, leftStickX: Int, leftStickY: Int, rightStickX: Int, rightStickY: Int): Boolean
    external fun collectGameInfo(rootDir: String, progressId: Long): Boolean
    external fun systemInfo(): String
    external fun settingsGet(path: String): String
    external fun settingsSet(path: String, value: String): Boolean
    external fun getState() : Int
    external fun kill()
    external fun resume()
    external fun openHomeMenu()
    external fun loginUser(userId: String)
    external fun getUser(): String
    external fun getTitleId(): String
    external fun supportsCustomDriverLoading() : Boolean
    external fun isInstallableFile(fd: Int) : Boolean
    external fun getDirInstallPath(sfoFd: Int) : String?
    external fun getVersion(): String
    external fun setCustomDriver(path: String, libraryName: String, hookDir: String): Boolean
    
    // Patch Manager native functions
    external fun applyGamePatches(patchFilePath: String, titleId: String, enabledPatches: String): Int
    external fun hasGamePatches(titleId: String): Boolean
    external fun getSupportedGamesForPatches(): String
    
    // ARMv9 Optimization Functions - NOT in native lib, stubbed
    fun initializeARMv9Optimizations(cacheDir: String, titleId: String, buildId: String): Boolean = false
    fun shutdownARMv9Optimizations() {}
    
    // NCE Mode control - NOT in native lib, stubbed
    // Settings are saved in GeneralSettings.nceMode
    fun setNCEMode(mode: Int) {}
    fun getNCEMode(): Int = 0
    fun isNCEActive(): Boolean = false
    fun isNCENativeActive(): Boolean = false
    
    // NCE Native - PS3 game loading (NOT in native lib, stubbed)
    fun loadAndStartGame(gamePath: String): Boolean = false
    fun stopGame() {}
    
    // Cache management - NOT in native lib, stubbed
    fun purgeAllCaches(cacheDir: String) {}
    fun getFastmemStats(): String = ""
    
    // JIT statistics - NOT in native lib, stubbed
    fun getJITStats(): String = ""
    fun runJITTest(): Boolean = false
    
    // RSX Graphics Engine - NOT in native lib, all stubbed
    fun rsxStart(): Boolean = false
    fun rsxStop(): Boolean = false
    fun rsxIsRunning(): Boolean = false
    fun rsxGetStats(): String = "RSX not available"
    fun getRSXFPS(): Int = 0
    fun rsxSetThreadCount(count: Int) {}
    fun rsxGetThreadCount(): Int = 0

    // Performance Benchmark Tests - NOT in native lib, stubbed
    fun runGraphicsPerformanceTest(numDraws: Int): Float = 0f
    fun runMemoryPerformanceTest(): Float = 0f
    fun runCPUPerformanceTest(): Float = 0f
    
    // Dynamic Resolution Scaling (DRS) Engine
    external fun drsInitialize(
        nativeWidth: Int, nativeHeight: Int,
        mode: Int, targetFps: Int,
        minScale: Float, maxScale: Float
    ): Boolean
    external fun drsShutdown()
    external fun drsIsActive(): Boolean
    external fun drsSetMode(mode: Int)
    external fun drsGetMode(): Int
    external fun drsSetTargetFPS(fps: Int)
    external fun drsGetTargetFPS(): Int
    external fun drsSetMinScale(scale: Float)
    external fun drsSetMaxScale(scale: Float)
    external fun drsUpdate(frameTimeMs: Float): Float
    external fun drsGetCurrentScale(): Float
    external fun drsGetStatsJson(): String
    external fun drsResetStats()
    external fun drsSetFSRUpscaling(enabled: Boolean)
    external fun drsIsFSRUpscalingEnabled(): Boolean
    
    // Texture Streaming Cache
    external fun textureStreamingInitialize(cacheSizeMb: Long, workerThreads: Int, mode: Int): Boolean
    external fun textureStreamingShutdown()
    external fun textureStreamingIsActive(): Boolean
    external fun textureStreamingSetMode(mode: Int)
    external fun textureStreamingGetMode(): Int
    external fun textureStreamingGetStatsJson(): String
    external fun textureStreamingClearCache()
    
    // SVE2 Optimizations
    external fun sve2Initialize(): Boolean
    external fun sve2Shutdown()
    external fun sve2IsActive(): Boolean
    external fun sve2HasFeature(feature: Int): Boolean
    external fun sve2GetVectorLength(): Int
    external fun sve2GetCapabilitiesJson(): String
    
    // Pipeline Cache
    external fun pipelineCacheInitialize(maxPipelines: Int, compileThreads: Int, cachePath: String): Boolean
    external fun pipelineCacheShutdown()
    external fun pipelineCacheIsActive(): Boolean
    external fun pipelineCacheClear()
    external fun pipelineCacheSave(path: String): Boolean
    external fun pipelineCacheLoad(path: String): Boolean
    external fun pipelineCacheGetStatsJson(): String
    
    // Game Profiles
    external fun profilesInitialize(profilesDir: String): Boolean
    external fun profilesShutdown()
    external fun profilesIsActive(): Boolean
    external fun profilesApply(titleId: String): Boolean
    external fun profilesHasProfile(titleId: String): Boolean
    external fun profilesDelete(titleId: String): Boolean
    external fun profilesExport(titleId: String): String
    external fun profilesGetGameName(titleId: String): String
    external fun profilesGetGameRegion(titleId: String): String
    external fun profilesGetStatsJson(): String

    // PS3 Patch Installer
    external fun patchInstallerIsActive(): Boolean
    external fun patchInstallerGetPatches(titleId: String): String
    external fun patchInstallerApplyRecommended(titleId: String, memBase: Long, memSize: Long): Int
    external fun patchInstallerGetStats(): String

    // Syscall Stubs
    external fun syscallStubsIsActive(): Boolean
    external fun syscallStubsGetStats(): String
    external fun syscallStubsExportLog(): String
    external fun syscallStubsReset()

    // Firmware Spoof
    external fun firmwareSpoofIsActive(): Boolean
    external fun firmwareSpoofSetVersion(major: Int, minor: Int)
    external fun firmwareSpoofGetVersion(): String
    external fun firmwareSpoofGetConfig(): String
    external fun firmwareSpoofGetRecommended(titleId: String): String

    // Library Emulation
    external fun libraryEmulationIsActive(): Boolean
    external fun libraryEmulationGetStats(): String
    external fun libraryEmulationIsAvailable(libName: String): Boolean
    external fun libraryEmulationGetStatus(libName: String): Int

    // Save Converter
    external fun saveConverterIsActive(): Boolean
    external fun saveConverterDetectFormat(path: String): Int
    external fun saveConverterDetectRegion(titleId: String): Int
    external fun saveConverterConvert(src: String, dst: String, format: Int): Boolean
    external fun saveConverterValidate(path: String): Boolean
    external fun saveConverterBackup(savePath: String, backupPath: String): Boolean
    external fun saveConverterCheckCompatibility(savePath: String, gameId: String): Boolean
    external fun saveConverterGetRegionName(region: Int): String
    external fun saveConverterGetStats(): String

    // =========================================================================
    // AGVSOL - Automatic GPU Vendor-Specific Optimization Layer
    // =========================================================================
    external fun agvsolInitialize(cacheDir: String): Boolean
    external fun agvsolShutdown()
    external fun agvsolIsInitialized(): Boolean
    external fun agvsolGetGPUInfo(): String
    external fun agvsolGetActiveProfile(): String
    external fun agvsolLoadProfile(path: String): Boolean
    external fun agvsolApplyProfile(): Boolean
    external fun agvsolExportProfile(): String
    external fun agvsolImportProfile(json: String): Boolean
    external fun agvsolGetGPUVendor(): Int
    external fun agvsolGetGPUTier(): Int
    external fun agvsolGetTargetFPS(): Int
    external fun agvsolGetResolutionScale(): Float
    external fun agvsolSetTargetFPS(fps: Int)
    external fun agvsolSetResolutionScale(scale: Float)
    external fun agvsolGetVendorFlag(key: String, defaultVal: Boolean): Boolean
    external fun agvsolGetStats(): String
    external fun getLastOpenLibraryError(): String


    companion object {
        var initialized = false
        var armv9OptimizationsEnabled = false
        val instance = RPCSX()
        var rootDirectory = ""
        var nativeLibDirectory = ""
        var lastPlayedGame = ""
        var activeGame = mutableStateOf<String?>(null)
        var state = mutableStateOf(EmulatorState.Stopped)
        var activeLibrary = mutableStateOf<String?>(null)
        var lastOpenError = mutableStateOf<String?>(null)
        
        /**
         * Ініціалізація ARMv9 оптимізацій для Snapdragon 8s Gen 3
         * NOTE: Native functions not available, just set flag
         */
        fun initializeOptimizations(cacheDir: String, titleId: String = ""): Boolean {
            if (armv9OptimizationsEnabled) {
                android.util.Log.i("RPCSX", "ARMv9 optimizations already enabled")
                return true
            }
            
            android.util.Log.i("RPCSX", "ARMv9 optimizations enabled (settings only)")
            armv9OptimizationsEnabled = true
            return true
        }
        
        /**
         * Вимкнення оптимізацій при виході
         */
        fun shutdownOptimizations() {
            if (armv9OptimizationsEnabled) {
                android.util.Log.i("RPCSX", "Shutting down ARMv9 optimizations")
                armv9OptimizationsEnabled = false
            }
        }
        
        /**
         * Встановлення режиму PPU декодера
         */
        fun setDecoderMode(mode: Int) {
            try {
                instance.setNCEMode(mode)
                android.util.Log.i("RPCSX", "PPU Decoder mode set to $mode")
            } catch (e: Throwable) {
                android.util.Log.e("RPCSX", "Failed to set PPU Decoder mode to $mode", e)
            }
        }

        fun boot(path: String): BootResult {
            return try {
                BootResult.fromInt(instance.boot(path))
            } catch (e: Throwable) {
                android.util.Log.e("RPCSX", "Failed to boot: ${e.message}", e)
                BootResult.GenericError
            }
        }

        fun updateState() {
            try {
                val newState = EmulatorState.fromInt(instance.getState())
                if (newState != state.value) {
                    state.value = newState
                }
            } catch (e: Throwable) {
                android.util.Log.e("RPCSX", "Failed to update state: ${e.message}", e)
            }
        }

        fun getState(): EmulatorState {
            updateState()
            return state.value
        }

        fun getHdd0Dir(): String {
            return rootDirectory + "config/dev_hdd0/"
        }

        fun openLibrary(path: String): Boolean {
            return try {
                if (!instance.openLibrary(path)) {
                    // Retrieve native dlopen error
                    try {
                        val nativeError = instance.getLastOpenLibraryError()
                        if (nativeError.isNotEmpty()) {
                            lastOpenError.value = nativeError
                            android.util.Log.e("RPCSX", "dlopen error: $nativeError")
                        }
                    } catch (_: Throwable) {}
                    return false
                }
                lastOpenError.value = null
                activeLibrary.value = path
                true
            } catch (e: Throwable) {
                lastOpenError.value = e.message
                android.util.Log.e("RPCSX", "Failed to open library: ${e.message}", e)
                false
            }
        }

        init {
            try {
                System.loadLibrary("rpcsx-android")
            } catch (e: Throwable) {
                android.util.Log.e("RPCSX", "Failed to load native library: ${e.message}", e)
            }
        }
    }
}
