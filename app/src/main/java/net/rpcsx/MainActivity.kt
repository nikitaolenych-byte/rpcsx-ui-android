package net.rpcsx

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.os.Bundle
import android.os.Environment
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.core.view.WindowCompat
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.launch
import net.rpcsx.dialogs.AlertDialogQueue
import net.rpcsx.ui.navigation.AppNavHost
import net.rpcsx.utils.GeneralSettings
import net.rpcsx.utils.GitHub
import net.rpcsx.utils.RpcsxUpdater
import net.rpcsx.utils.ApkInstaller
import java.io.File
import kotlin.concurrent.thread
import net.rpcsx.utils.safeNativeCall
import net.rpcsx.utils.safeSettingsSet
import net.rpcsx.media.CutsceneBridge

class MainActivity : ComponentActivity() {
    private lateinit var unregisterUsbEventListener: () -> Unit
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        GeneralSettings.init(this)

        // Initialize CutsceneBridge so native code can request playback
        try {
            CutsceneBridge.init(applicationContext)
        } catch (e: Throwable) {
            // Non-fatal
        }

        WindowCompat.setDecorFitsSystemWindows(window, false)

        if (!RPCSX.initialized) {
            try {
                Permission.PostNotifications.requestPermission(this)

                with(getSystemService(NOTIFICATION_SERVICE) as NotificationManager) {
                    val channel = NotificationChannel(
                        "rpcsx-progress",
                        getString(R.string.installation_progress),
                        NotificationManager.IMPORTANCE_DEFAULT
                    ).apply {
                        setShowBadge(false)
                        lockscreenVisibility = Notification.VISIBILITY_PUBLIC
                    }

                    createNotificationChannel(channel)
                }

                val externalDir = applicationContext.getExternalFilesDir(null)
                RPCSX.rootDirectory = (externalDir?.absolutePath ?: filesDir.absolutePath) + "/"

                // Load saved firmware info
                FirmwareRepository.load()

                GitHub.initialize(this)

            var rpcsxLibrary = GeneralSettings["rpcsx_library"] as? String
            val rpcsxUpdateStatus = GeneralSettings["rpcsx_update_status"]
            val rpcsxPrevLibrary = GeneralSettings["rpcsx_prev_library"] as? String

            if (rpcsxLibrary != null) {
                var skipLoading = false

                if (rpcsxUpdateStatus == false && rpcsxPrevLibrary != null) {
                    // Update failed, have previous version to rollback to
                    val loadAttempts = (GeneralSettings["rpcsx_load_attempts"] as? Int) ?: 0
                    
                    if (loadAttempts >= 2) {
                        // Already tried multiple times - mark as bad version and stop trying
                        Log.e("RPCSX", "Multiple load failures for $rpcsxLibrary, marking as bad version")
                        GeneralSettings["rpcsx_bad_version"] = RpcsxUpdater.getFileVersion(File(rpcsxLibrary))
                        GeneralSettings["rpcsx_load_attempts"] = 0
                        GeneralSettings["rpcsx_update_status"] = true
                        GeneralSettings.sync()
                        skipLoading = true
                        
                        AlertDialogQueue.showDialog(
                            getString(R.string.failed_to_update_rpcsx),
                            "Library failed to load after multiple attempts. Please check if your device supports this version."
                        )
                    } else {
                        // First or second failure - rollback to previous
                        GeneralSettings["rpcsx_library"] = rpcsxPrevLibrary
                        GeneralSettings["rpcsx_installed_arch"] = GeneralSettings["rpcsx_prev_installed_arch"]
                        GeneralSettings["rpcsx_prev_installed_arch"] = null
                        GeneralSettings["rpcsx_prev_library"] = null
                        GeneralSettings["rpcsx_bad_version"] = RpcsxUpdater.getFileVersion(File(rpcsxLibrary))
                        GeneralSettings["rpcsx_load_attempts"] = 0
                        GeneralSettings.sync()

                        File(rpcsxLibrary).delete()
                        rpcsxLibrary = rpcsxPrevLibrary

                        AlertDialogQueue.showDialog(
                            getString(R.string.failed_to_update_rpcsx),
                            getString(R.string.failed_to_load_new_version)
                        )
                    }
                } else if (rpcsxUpdateStatus == false && rpcsxPrevLibrary == null) {
                    // First install that previously crashed/failed (no previous version to rollback to)
                    val loadAttempts = (GeneralSettings["rpcsx_load_attempts"] as? Int) ?: 0
                    Log.w("RPCSX", "First install, status=false, prevLib=null, attempts=$loadAttempts")
                    
                    if (loadAttempts >= 2) {
                        // Multiple crashes/failures with no fallback - give up on this version
                        Log.e("RPCSX", "Library failed to load after $loadAttempts attempts (first install, no rollback)")
                        GeneralSettings["rpcsx_bad_version"] = RpcsxUpdater.getFileVersion(File(rpcsxLibrary))
                        GeneralSettings["rpcsx_load_attempts"] = 0
                        GeneralSettings["rpcsx_update_status"] = true
                        GeneralSettings["rpcsx_library"] = null
                        GeneralSettings.sync()
                        skipLoading = true
                        
                        AlertDialogQueue.showDialog(
                            getString(R.string.failed_to_update_rpcsx),
                            "Library failed to load after multiple attempts. This version may not be compatible with your device."
                        )
                    } else {
                        // Increment attempts BEFORE openLibrary (in case of crash)
                        GeneralSettings["rpcsx_load_attempts"] = loadAttempts + 1
                        GeneralSettings.sync()
                    }
                } else if (rpcsxUpdateStatus == null) {
                    // Fresh install/update - first attempt after installUpdate()
                    val loadAttempts = (GeneralSettings["rpcsx_load_attempts"] as? Int) ?: 0
                    GeneralSettings["rpcsx_update_status"] = false
                    GeneralSettings["rpcsx_load_attempts"] = loadAttempts + 1
                    GeneralSettings.sync()
                }

                if (!skipLoading) {
                    // Check if library file exists before trying to load
                    val libraryFile = File(rpcsxLibrary!!)
                    if (!libraryFile.exists()) {
                        Log.e("RPCSX", "Library file does not exist: $rpcsxLibrary")
                        // If file doesn't exist and we have previous library, rollback
                        if (rpcsxPrevLibrary != null && File(rpcsxPrevLibrary).exists()) {
                            GeneralSettings["rpcsx_library"] = rpcsxPrevLibrary
                            GeneralSettings["rpcsx_installed_arch"] = GeneralSettings["rpcsx_prev_installed_arch"]
                            GeneralSettings["rpcsx_prev_installed_arch"] = null
                            GeneralSettings["rpcsx_prev_library"] = null
                            GeneralSettings.sync()
                            rpcsxLibrary = rpcsxPrevLibrary
                        }
                    }

                    if (RPCSX.openLibrary(rpcsxLibrary!!)) {
                        // Library loaded successfully - mark update as successful
                        if (GeneralSettings["rpcsx_update_status"] == false) {
                            GeneralSettings["rpcsx_update_status"] = true
                            GeneralSettings["rpcsx_prev_library"] = null
                            GeneralSettings["rpcsx_prev_installed_arch"] = null
                            GeneralSettings["rpcsx_load_attempts"] = 0
                            GeneralSettings.sync()
                            Log.i("RPCSX", "Library update successful: $rpcsxLibrary")
                        }
                    } else {
                        Log.e("RPCSX", "Failed to open library: $rpcsxLibrary")
                        // Don't leave update_status as null - set to false so we can handle it next time
                        if (GeneralSettings["rpcsx_update_status"] == null) {
                            GeneralSettings["rpcsx_update_status"] = false
                            GeneralSettings.sync()
                        }
                    }
                }
            }

            val nativeLibraryDir =
                packageManager.getApplicationInfo(packageName, 0).nativeLibraryDir
            RPCSX.nativeLibDirectory = nativeLibraryDir

            if (RPCSX.activeLibrary.value != null) {
                try {
                    RPCSX.instance.initialize(RPCSX.rootDirectory, UserRepository.getUserFromSettings())
                } catch (e: Throwable) {
                    Log.e("RPCSX", "Failed to initialize RPCSX", e)
                }
                val gpuDriverPath = GeneralSettings["gpu_driver_path"] as? String
                val gpuDriverName = GeneralSettings["gpu_driver_name"] as? String

                if (gpuDriverPath != null && gpuDriverName != null) {
                    try {
                        RPCSX.instance.setCustomDriver(gpuDriverPath, gpuDriverName, nativeLibraryDir)
                    } catch (e: Throwable) {
                        Log.e("RPCSX", "Failed to set custom driver", e)
                    }
                }

                // DISABLED: Performance toggles were causing crashes
                // applyStartupPerformanceToggles()

                lifecycleScope.launch {
                    UserRepository.load()
                    GameRepository.load()
                }

                // Attempt to restore safe decoder settings on startup if native library present.
                try {
                    val restored = net.rpcsx.utils.DecoderUtils.restoreDecodersToSafeDefaults()
                    if (!restored) Log.i("RPCSX", "Decoder restore reported failure or not applicable")
                } catch (e: Throwable) {
                    Log.e("RPCSX", "Error restoring decoders on startup", e)
                }

                RPCSX.initialized = true

                thread {
                    try {
                        RPCSX.instance.startMainThreadProcessor()
                    } catch (e: Throwable) {
                        Log.e("RPCSX", "Failed to start main thread processor", e)
                    }
                }

                thread {
                    try {
                        RPCSX.instance.processCompilationQueue()
                    } catch (e: Throwable) {
                        Log.e("RPCSX", "Failed to process compilation queue", e)
                    }
                }

                GeneralSettings["rpcsx_update_status"] = true
                if (rpcsxPrevLibrary != null) {
                    if (rpcsxLibrary != rpcsxPrevLibrary) {
                        File(rpcsxPrevLibrary).delete()
                    }

                    GeneralSettings["rpcsx_prev_library"] = null
                    GeneralSettings["rpcsx_prev_installed_arch"] = null
                    GeneralSettings.sync()
                }
            }

            val updateFile = File(RPCSX.rootDirectory + "cache", "rpcsx-${BuildConfig.Version}.apk")
            if (updateFile.exists()) {
                updateFile.delete()
            }
            } catch (e: Throwable) {
                Log.e("RPCSX", "Failed to initialize: ${e.message}", e)
            }
        }

        setContent {
            RPCSXTheme {
                AppNavHost()
            }
        }

        try {
            if (RPCSX.activeLibrary.value != null) {
                unregisterUsbEventListener = listenUsbEvents(this)
            } else {
                unregisterUsbEventListener = {}
            }
        } catch (e: Throwable) {
            Log.e("RPCSX", "Failed to setup USB listener", e)
            unregisterUsbEventListener = {}
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        try {
            if (::unregisterUsbEventListener.isInitialized) {
                unregisterUsbEventListener()
            }
        } catch (e: Throwable) {
            Log.e("RPCSX", "Failed to unregister USB listener", e)
        }
    }

    /**
     * Auto-enable NCE/JIT on first launch for ARMv9 optimization.
     * PPU modules MUST be compiled with LLVM, then NCE optimizes execution.
     * NOTE: setNCEMode not available in native library - settings only
     */
    private fun autoEnableNceJit() {
        val nceEnabledKey = "nce_jit_auto_enabled"
        val nceMode = GeneralSettings.nceMode
        
        // NCE mode saved in preferences only - native function not available
        // if (nceMode >= 0) {
        //     try {
        //         RPCSX.instance.setNCEMode(nceMode)
        //         Log.i("RPCSX", "Restored NCE mode from preferences: $nceMode")
        //     } catch (e: Throwable) {
        //         Log.e("RPCSX", "Error restoring NCE mode", e)
        //     }
        // }
        
        // First launch - auto-enable NCE/JIT settings
        if (GeneralSettings[nceEnabledKey] != true) {
            try {
                // IMPORTANT: Use LLVM Recompiler to compile PPU modules!
                val ok = safeSettingsSet("Core@@PPU Decoder", "\"LLVM Recompiler (Legacy)\"")
                if (ok) {
                    // Save NCE mode to preferences (native function not available)
                    // try {
                    //     RPCSX.instance.setNCEMode(3)
                    // } catch (e: Throwable) {
                    //     Log.e("RPCSX", "Error setting NCE mode", e)
                    // }
                    GeneralSettings.nceMode = 3
                    GeneralSettings[nceEnabledKey] = true
                    Log.i("RPCSX", "NCE/JIT settings saved (LLVM + NCE optimization)")
                } else {
                    Log.w("RPCSX", "Failed to set PPU Decoder to LLVM, trying fallback...")
                    // Save NCE settings
                    // try {
                    //     RPCSX.instance.setNCEMode(3)
                    // } catch (e: Throwable) {
                    //     Log.e("RPCSX", "Error setting NCE mode", e)
                    // }
                    GeneralSettings.nceMode = 3
                    GeneralSettings[nceEnabledKey] = true
                }
            } catch (e: Throwable) {
                Log.e("RPCSX", "Error auto-enabling NCE/JIT", e)
            }
        }
    }

    /**
     * Apply maximum performance defaults (one-time unless user disables it).
     * Keeps user overrides if they explicitly turned auto mode off.
     */
    private fun applyAutoMaxPerformance() {
        val autoKey = "auto_max_performance"
        val autoEnabled = (GeneralSettings[autoKey] as? Boolean) ?: true
        if (GeneralSettings[autoKey] == null) {
            GeneralSettings[autoKey] = true
        }

        if (!autoEnabled) return

        try {
            // RSX defaults for best performance
            GeneralSettings.setValue("rsx_enabled", true)
            GeneralSettings.setValue("rsx_thread_count", 8)
            GeneralSettings.setValue("rsx_resolution_scale", 50)
            GeneralSettings.setValue("rsx_vsync", false)
            GeneralSettings.setValue("rsx_frame_limit", false)
            GeneralSettings.setValue("max_perf_defaults_applied", true)
            GeneralSettings.sync()

            // Apply RSX thread count to native layer (takes effect on next init)
                net.rpcsx.utils.safeNativeCall { RPCSX.instance.rsxSetThreadCount(8) }

            // LLVM performance tweaks
            safeSettingsSet("Core@@PPU LLVM Greedy Mode", "true")
            safeSettingsSet("Core@@LLVM Precompilation", "true")
            safeSettingsSet("Core@@Set DAZ and FTZ", "true")
            safeSettingsSet("Core@@Max LLVM Compile Threads", "16")
        } catch (e: Throwable) {
            Log.e("RPCSX", "Failed to apply max performance defaults", e)
        }
    }

    // Use shared safeSettingsSet from net.rpcsx.utils

    /**
     * Enable native ARMv9 optimizations if supported by device.
     */
    private fun enableArmv9Optimizations() {
        try {
            val cacheDir = File(RPCSX.rootDirectory, "cache")
            if (!cacheDir.exists()) {
                cacheDir.mkdirs()
            }
            RPCSX.initializeOptimizations(cacheDir.absolutePath)
        } catch (e: Throwable) {
            Log.e("RPCSX", "Failed to enable ARMv9 optimizations", e)
        }
    }

    private fun applyStartupPerformanceToggles() {
        try {
            autoEnableNceJit()
        } catch (e: Throwable) {
            Log.e("RPCSX", "Startup: NCE/JIT toggle failed", e)
        }

        try {
            applyAutoMaxPerformance()
        } catch (e: Throwable) {
            Log.e("RPCSX", "Startup: Max performance toggle failed", e)
        }

        try {
            enableArmv9Optimizations()
        } catch (e: Throwable) {
            Log.e("RPCSX", "Startup: ARMv9 optimizations failed", e)
        }
    }

    /**
     * Check for APK in Downloads folder and offer to install.
     */
    private fun checkAndInstallApkFromDownloads() {
        try {
            val downloadDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
            val apkNames = listOf("app-release.apk", "rpcsx-release.apk", "rpcsx-armv9.apk")
            for (name in apkNames) {
                val apkFile = File(downloadDir, name)
                if (apkFile.exists() && apkFile.canRead()) {
                    AlertDialogQueue.showDialog(
                        title = getString(R.string.install_custom_rpcsx_lib),
                        message = "Found $name in Downloads. Install it?",
                        onConfirm = {
                            ApkInstaller.installApk(this, apkFile)
                        }
                    )
                    break
                }
            }
        } catch (e: Throwable) {
            Log.e("RPCSX", "Error checking Downloads for APK", e)
        }
    }
}
