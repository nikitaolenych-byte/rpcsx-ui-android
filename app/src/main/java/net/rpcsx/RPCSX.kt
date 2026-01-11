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
    
    // ARMv9 Optimization Functions
    external fun initializeARMv9Optimizations(cacheDir: String, titleId: String, buildId: String): Boolean
    external fun shutdownARMv9Optimizations()


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
        
        /**
         * Ініціалізація ARMv9 оптимізацій для Snapdragon 8s Gen 3
         */
        fun initializeOptimizations(cacheDir: String, titleId: String = ""): Boolean {
            if (armv9OptimizationsEnabled) {
                android.util.Log.i("RPCSX", "ARMv9 optimizations already enabled")
                return true
            }
            
            android.util.Log.i("RPCSX", "Enabling ARMv9 optimizations for Snapdragon 8s Gen 3")
            val buildId = "${BuildConfig.VERSION_NAME}:${BuildConfig.VERSION_CODE}"
            armv9OptimizationsEnabled = instance.initializeARMv9Optimizations(cacheDir, titleId, buildId)
            
            if (armv9OptimizationsEnabled) {
                android.util.Log.i("RPCSX", "ARMv9 optimizations enabled successfully!")
            } else {
                android.util.Log.e("RPCSX", "Failed to enable ARMv9 optimizations")
            }
            
            return armv9OptimizationsEnabled
        }
        
        /**
         * Вимкнення оптимізацій при виході
         */
        fun shutdownOptimizations() {
            if (armv9OptimizationsEnabled) {
                android.util.Log.i("RPCSX", "Shutting down ARMv9 optimizations")
                instance.shutdownARMv9Optimizations()
                armv9OptimizationsEnabled = false
            }
        }

        fun boot(path: String): BootResult {
            return BootResult.fromInt(instance.boot(path))
        }

        fun updateState() {
            val newState = EmulatorState.fromInt(instance.getState())
            if (newState != state.value) {
                state.value = newState
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
            if (!instance.openLibrary(path)) {
                return false
            }

            activeLibrary.value = path
            return true
        }

        init {
            System.loadLibrary("rpcsx-android")
        }
    }
}
