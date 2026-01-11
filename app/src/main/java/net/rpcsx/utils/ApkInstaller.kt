package net.rpcsx.utils

import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageInstaller
import android.net.Uri
import android.os.Build
import android.provider.Settings
import android.util.Log
import androidx.core.content.FileProvider
import androidx.core.net.toUri
import net.rpcsx.R
import net.rpcsx.dialogs.AlertDialogQueue
import java.io.File

/**
 * APK installation utility with multiple fallback methods.
 */
object ApkInstaller {

    private const val TAG = "ApkInstaller"

    /**
     * Install APK using the best available method.
     * Tries PackageInstaller first, then falls back to Intent.ACTION_VIEW.
     */
    fun installApk(context: Context, apkFile: File): Boolean {
        if (!apkFile.exists()) {
            Log.e(TAG, "APK file does not exist: ${apkFile.absolutePath}")
            return false
        }

        // Check for install permission first
        if (!context.packageManager.canRequestPackageInstalls()) {
            requestInstallPermission(context)
            return false
        }

        return try {
            installViaPackageInstaller(context, apkFile)
        } catch (e: Exception) {
            Log.w(TAG, "PackageInstaller failed, trying Intent.ACTION_VIEW", e)
            installViaIntent(context, apkFile)
        }
    }

    /**
     * Request permission to install from unknown sources.
     */
    private fun requestInstallPermission(context: Context) {
        AlertDialogQueue.showDialog(
            title = context.getString(R.string.permission_required),
            message = context.getString(R.string.permission_required_description),
            onConfirm = {
                val intent = Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES)
                    .setData("package:${context.packageName}".toUri())
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                context.startActivity(intent)
            }
        )
    }

    /**
     * Install APK using PackageInstaller API (preferred for Android 5+).
     */
    private fun installViaPackageInstaller(context: Context, apkFile: File): Boolean {
        Log.i(TAG, "Installing via PackageInstaller: ${apkFile.absolutePath}")

        val intent = Intent(context, PackageInstallStatusReceiver::class.java)
        val pendingIntent = PendingIntent.getBroadcast(
            context,
            3456,
            intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_MUTABLE
        )

        apkFile.inputStream().use { apkStream ->
            val installer = context.packageManager.packageInstaller
            val length = apkFile.length()

            val params = PackageInstaller.SessionParams(
                PackageInstaller.SessionParams.MODE_FULL_INSTALL
            )
            val sessionId = installer.createSession(params)

            installer.openSession(sessionId).use { session ->
                session.openWrite(apkFile.name, 0, length).use { sessionStream ->
                    apkStream.copyTo(sessionStream)
                    session.fsync(sessionStream)
                }
                session.commit(pendingIntent.intentSender)
            }
        }

        return true
    }

    /**
     * Fallback: Install APK using Intent.ACTION_VIEW.
     * Works when PackageInstaller fails or for simpler scenarios.
     */
    private fun installViaIntent(context: Context, apkFile: File): Boolean {
        Log.i(TAG, "Installing via Intent.ACTION_VIEW: ${apkFile.absolutePath}")

        val apkUri: Uri = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            // Use FileProvider for Android 7+
            FileProvider.getUriForFile(
                context,
                "${context.packageName}.fileprovider",
                apkFile
            )
        } else {
            Uri.fromFile(apkFile)
        }

        val intent = Intent(Intent.ACTION_VIEW).apply {
            setDataAndType(apkUri, "application/vnd.android.package-archive")
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        }

        return try {
            context.startActivity(intent)
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start install intent", e)
            AlertDialogQueue.showDialog(
                context.getString(R.string.error),
                "Failed to install APK: ${e.message}"
            )
            false
        }
    }

    /**
     * Install APK from URI (useful for content:// URIs from file pickers).
     */
    fun installApkFromUri(context: Context, apkUri: Uri): Boolean {
        if (!context.packageManager.canRequestPackageInstalls()) {
            requestInstallPermission(context)
            return false
        }

        val intent = Intent(Intent.ACTION_VIEW).apply {
            setDataAndType(apkUri, "application/vnd.android.package-archive")
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        }

        return try {
            context.startActivity(intent)
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to install APK from URI", e)
            false
        }
    }
}
