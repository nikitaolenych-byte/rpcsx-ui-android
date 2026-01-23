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

class MainActivity : ComponentActivity() {
    private lateinit var unregisterUsbEventListener: () -> Unit
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        GeneralSettings.init(this)

        WindowCompat.setDecorFitsSystemWindows(window, false)

        if (!RPCSX.initialized) {
            Permission.PostNotifications.requestPermission(this)

            with(getSystemService(NOTIFICATION_SERVICE) as NotificationManager) {
                val channel = NotificationChannel(
                    "rpcsx-progress",
                    getString(R.string.installation_progress),
                    NotificationManager.IMPORTANCE_DEFAULT
                try {
                    setShowBadge(false)
                    lockscreenVisibility = Notification.VISIBILITY_PUBLIC
                }

                createNotificationChannel(channel)
            }

            RPCSX.rootDirectory = applicationContext.getExternalFilesDir(null).toString()
            if (!RPCSX.rootDirectory.endsWith("/")) {
                RPCSX.rootDirectory += "/"
                    if (RPCSX.activeLibrary.value != null) {
                        try {
                            RPCSX.instance.rsxSetThreadCount(8)
                        } catch (e: Throwable) {
                            Log.e("RPCSX", "Failed to set RSX thread count", e)
                        }
                    } else {
                        Log.w("RPCSX", "Skipping rsxSetThreadCount: native library not loaded")
                    }
                } catch (e: Throwable) {
                    Log.e("RPCSX", "Failed to apply max performance defaults", e)
            GitHub.initialize(this)

            var rpcsxLibrary = GeneralSettings["rpcsx_library"] as? String
            val rpcsxUpdateStatus = GeneralSettings["rpcsx_update_status"]
            val rpcsxPrevLibrary = GeneralSettings["rpcsx_prev_library"] as? String

            if (rpcsxLibrary != null) {
                if (rpcsxUpdateStatus == false && rpcsxPrevLibrary != null) {
                    GeneralSettings["rpcsx_library"] = rpcsxPrevLibrary
                    GeneralSettings["rpcsx_installed_arch"] = GeneralSettings["rpcsx_prev_installed_arch"]
                    GeneralSettings["rpcsx_prev_installed_arch"] = null
                    GeneralSettings["rpcsx_prev_library"] = null
                    GeneralSettings["rpcsx_bad_version"] = RpcsxUpdater.getFileVersion(File(rpcsxLibrary))
                    GeneralSettings.sync()

                    File(rpcsxLibrary).delete()
                    rpcsxLibrary = rpcsxPrevLibrary

                    AlertDialogQueue.showDialog(
                        getString(R.string.failed_to_update_rpcsx),
                        getString(R.string.failed_to_load_new_version)
                    )
                } else if (rpcsxUpdateStatus == null) {
                    GeneralSettings["rpcsx_update_status"] = false
                    GeneralSettings.sync()
                }

                RPCSX.openLibrary(rpcsxLibrary)
            }

            val nativeLibraryDir =
                packageManager.getApplicationInfo(packageName, 0).nativeLibraryDir
            RPCSX.nativeLibDirectory = nativeLibraryDir

            if (RPCSX.activeLibrary.value != null) {
                RPCSX.instance.initialize(RPCSX.rootDirectory, UserRepository.getUserFromSettings())
                val gpuDriverPath = GeneralSettings["gpu_driver_path"] as? String
                val gpuDriverName = GeneralSettings["gpu_driver_name"] as? String

                if (gpuDriverPath != null && gpuDriverName != null) {
                    RPCSX.instance.setCustomDriver(gpuDriverPath, gpuDriverName, nativeLibraryDir)
                }

                // Auto-enable NCE/JIT on first launch for ARMv9 optimization
                autoEnableNceJit()

                // Apply maximum performance defaults (RSX + settings)
                applyAutoMaxPerformance()

                // Enable ARMv9 native optimizations when available
                enableArmv9Optimizations()

                lifecycleScope.launch {
                    UserRepository.load()
                }

                RPCSX.initialized = true

                thread {
                    RPCSX.instance.startMainThreadProcessor()
                }

                thread {
                    RPCSX.instance.processCompilationQueue()
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
        }

        setContent {
            RPCSXTheme {
                AppNavHost()
            }
        }

        if (RPCSX.activeLibrary.value != null) {
            unregisterUsbEventListener = listenUsbEvents(this)
        } else {
            unregisterUsbEventListener = {}
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        unregisterUsbEventListener()
    }

    /**
     * Auto-enable NCE/JIT on first launch for ARMv9 optimization.
     * PPU modules MUST be compiled with LLVM, then NCE optimizes execution.
     */
    private fun autoEnableNceJit() {
        val nceEnabledKey = "nce_jit_auto_enabled"
        val nceMode = GeneralSettings.nceMode
        
        // Always restore NCE mode from saved preference
        if (nceMode >= 0) {
            RPCSX.instance.setNCEMode(nceMode)
            Log.i("RPCSX", "Restored NCE mode from preferences: $nceMode")
        }
        
        // First launch - auto-enable NCE/JIT
        if (GeneralSettings[nceEnabledKey] != true) {
            try {
                // IMPORTANT: Use LLVM Recompiler to compile PPU modules!
                // Interpreter does NOT compile PPU modules - they just get skipped.
                // NCE optimizes the already-compiled LLVM code at runtime.
                val ok = RPCSX.instance.settingsSet("Core@@PPU Decoder", "\"LLVM Recompiler (Legacy)\"")
                if (ok) {
                    // Activate NCE JIT layer (mode 3) for additional ARM64 optimizations
                    RPCSX.instance.setNCEMode(3)
                    GeneralSettings.nceMode = 3
                    GeneralSettings[nceEnabledKey] = true
                    Log.i("RPCSX", "NCE/JIT auto-enabled (LLVM + NCE optimization layer)")
                } else {
                    Log.w("RPCSX", "Failed to set PPU Decoder to LLVM, trying fallback...")
                    // Still enable NCE for runtime optimizations
                    RPCSX.instance.setNCEMode(3)
                    GeneralSettings.nceMode = 3
                    GeneralSettings[nceEnabledKey] = true
                }
            } catch (e: Exception) {
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
            try {
                RPCSX.instance.rsxSetThreadCount(8)
            } catch (e: Exception) {
                Log.e("RPCSX", "Failed to set RSX thread count", e)
            }

            // LLVM performance tweaks
            safeSettingsSet("Core@@PPU LLVM Greedy Mode", "true")
            safeSettingsSet("Core@@LLVM Precompilation", "true")
            safeSettingsSet("Core@@Set DAZ and FTZ", "true")
            safeSettingsSet("Core@@Max LLVM Compile Threads", "16")
        } catch (e: Exception) {
            Log.e("RPCSX", "Failed to apply max performance defaults", e)
        }
    }

    private fun safeSettingsSet(path: String, value: String): Boolean {
        return try {
            RPCSX.instance.settingsSet(path, value)
        } catch (e: Exception) {
            Log.e("RPCSX", "Error setting $path to $value", e)
            false
        }
    }

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
        } catch (e: Exception) {
            Log.e("RPCSX", "Failed to enable ARMv9 optimizations", e)
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
        } catch (e: Exception) {
            Log.e("RPCSX", "Error checking Downloads for APK", e)
        }
    }
}
