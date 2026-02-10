package net.rpcsx.utils

import android.content.Context
import android.content.Intent
import android.os.Build
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import net.rpcsx.R
import net.rpcsx.RPCSX
import net.rpcsx.utils.safeNativeCall
import net.rpcsx.dialogs.AlertDialogQueue
import net.rpcsx.ui.channels.DevRpcsxChannel
import java.io.File
import kotlin.system.exitProcess


object RpcsxUpdater {
    fun getCurrentVersion(): String? {
        if (RPCSX.activeLibrary.value == null) {
            return null
        }

        val ver = safeNativeCall { RPCSX.instance.getVersion() } ?: return null
        return "v" + ver.trim().removeSuffix(" Draft").trim() + "-" + GeneralSettings["rpcsx_installed_arch"]
    }

    fun getFileArch(file: File): String? {
        val parts = file.name.removeSuffix(".so").split("_")
        if (parts.size != 3) {
            return null
        }

        return parts[1]
    }
    fun getFileVersion(file: File): String? {
        val parts = file.name.removeSuffix(".so").split("_")
        if (parts.size != 3) {
            return null
        }
        val arch = parts[1]
        val version = parts[2]
        return "$version-$arch"
    }

    fun getAbi(): String = Build.SUPPORTED_64_BIT_ABIS[0]

    fun getArch(): String {
        return when (getAbi()) {
            "x86_64" -> "x86-64"
            else -> GeneralSettings["rpcsx_arch"] as? String ?: "armv8-a"
        }
    }

    fun setArch(arch: String) {
        GeneralSettings["rpcsx_arch"] = arch
    }

    suspend fun checkForUpdate(): String? {
        val url = DevRpcsxChannel // TODO: update once RPCSX has release with android support

        val arch = getArch()
        
        // Check if we already have a library downloaded (even if not loaded yet)
        val existingLibrary = GeneralSettings["rpcsx_library"] as? String
        val existingLibraryExists = existingLibrary != null && File(existingLibrary).exists()
        
        when (val fetchResult = GitHub.fetchLatestRelease(url)) {
            is GitHub.FetchResult.Success<*> -> {
                val release = fetchResult.content as GitHub.Release
                val releaseVersion = "${release.name}-${arch}"

                if (release.assets.find { it.name == "librpcsx-android-${getAbi()}-${arch}.so" }?.browser_download_url == null) {
                    return null
                }

                // If library file exists on disk, don't prompt for download again
                // (even if activeLibrary is null due to load failure)
                if (existingLibraryExists) {
                    Log.i("RPCSX-Updater", "Library already exists at $existingLibrary, checking version")
                    val existingVersion = getFileVersion(File(existingLibrary))
                    Log.i("RPCSX-Updater", "existingVersion=$existingVersion, releaseVersion=$releaseVersion, badVersion=${GeneralSettings["rpcsx_bad_version"]}")
                    if (existingVersion == releaseVersion || releaseVersion == GeneralSettings["rpcsx_bad_version"]) {
                        return null // Already have this version
                    }
                    // If activeLibrary is null (load failed) but file exists,
                    // don't re-download - the problem is loading, not downloading
                    if (RPCSX.activeLibrary.value == null) {
                        Log.w("RPCSX-Updater", "Library exists but failed to load. Not prompting re-download.")
                        return null
                    }
                }

                if (RPCSX.activeLibrary.value == null && !existingLibraryExists) {
                    return releaseVersion
                }

                if (getCurrentVersion() != releaseVersion && releaseVersion != GeneralSettings["rpcsx_bad_version"]) {
                    return releaseVersion
                }
            }
            is GitHub.FetchResult.Error -> {
//                AlertDialogQueue.showDialog("Check For RPCSX Updates Error", fetchResult.message)
            }
        }

        return null
    }

    suspend fun downloadUpdate(destinationDir: File, progressCallback: (Long, Long) -> Unit): File? {
        val url = DevRpcsxChannel // TODO: GeneralSettings["rpcsx_channel"] as String
        val arch = getArch()

        when (val fetchResult = GitHub.fetchLatestRelease(url)) {
            is GitHub.FetchResult.Success<*> -> {
                val release = fetchResult.content as GitHub.Release
                val releaseVersion = "${release.name}-${arch}"
                val releaseAsset = release.assets.find { it.name == "librpcsx-android-${getAbi()}-$arch.so" }

                if (releaseVersion != getCurrentVersion() && releaseAsset?.browser_download_url != null) {
                    val target = File(destinationDir, "librpcsx-android_${arch}_${release.name}.so")

                    if (target.exists()) {
                        return target
                    }

                    val tmp = File(destinationDir, "librpcsx.so.tmp")
                    if (tmp.exists()) {
                        withContext(Dispatchers.IO) {
                            tmp.delete()
                        }
                    }

                    withContext(Dispatchers.IO) {
                        tmp.createNewFile()
                    }

                    tmp.deleteOnExit()

                    when (val downloadStatus = GitHub.downloadAsset(releaseAsset.browser_download_url, tmp, progressCallback)) {
                        is GitHub.DownloadStatus.Success -> {
                            withContext(Dispatchers.IO) {
                                tmp.renameTo(target)
                            }
                            return target
                        }
                        is GitHub.DownloadStatus.Error ->
                            AlertDialogQueue.showDialog("RPCSX Download Error", downloadStatus.message ?: "Unexpected error")
                    }
                }
            }
            is GitHub.FetchResult.Error -> {
                AlertDialogQueue.showDialog("RPCSX Download Error", fetchResult.message)
            }
        }

        return null
    }

    fun installUpdate(context: Context, updateFile: File): Boolean {
        val restart = {
            try {
                val packageManager = context.packageManager
                var intent = packageManager.getLaunchIntentForPackage(context.packageName)
                if (intent == null) {
                    intent = Intent(Intent.ACTION_MAIN)
                    intent.addCategory(Intent.CATEGORY_LAUNCHER)
                    intent.setPackage(context.packageName)
                }
                // Ensure a clean task and new task flags so activity restarts properly
                intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
                context.startActivity(intent)
                GeneralSettings.sync()
                // Allow activity to start before force-exiting the process
                android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
                    try {
                        exitProcess(0)
                    } catch (e: Throwable) {
                        Log.e("RPCSX-UI", "exit failed: ${e.message}")
                    }
                }, 300)
            } catch (e: Throwable) {
                Log.e("RPCSX-UI", "Failed to restart app: ${e.message}")
            }
        }

        val prevLibrary = GeneralSettings["rpcsx_library"] as? String
        val prevArch = GeneralSettings["rpcsx_installed_arch"] as? String
        GeneralSettings["rpcsx_library"] = updateFile.toString()
        GeneralSettings["rpcsx_update_status"] = null
        GeneralSettings["rpcsx_installed_arch"] = getFileArch(updateFile)
        GeneralSettings["rpcsx_load_attempts"] = 0

        Log.i("RPCSX-UI", "Registered update file ${GeneralSettings["rpcsx_library"]}")

        // Guard: never set prev_library to the same path as new library (self-reference causes loop)
        if (prevLibrary != null && prevLibrary != updateFile.toString()) {
            GeneralSettings["rpcsx_prev_library"] = prevLibrary
            GeneralSettings["rpcsx_prev_installed_arch"] = prevArch
        } else {
            Log.w("RPCSX-UI", "Skipping prev_library: same as new or null (prev=$prevLibrary)")
            GeneralSettings["rpcsx_prev_library"] = null
            GeneralSettings["rpcsx_prev_installed_arch"] = null
        }
        
        // Sync settings to disk BEFORE showing dialog to ensure changes are persisted
        GeneralSettings.sync()
        
        AlertDialogQueue.showDialog(
            title = context.getString(R.string.rpcsx_update_available),
            message = context.getString(R.string.restart_ui_to_apply_change),
            onConfirm = { restart() }
        )
        return true
    }
}
