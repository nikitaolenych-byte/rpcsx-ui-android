package net.rpcsx

import android.content.ContentProvider
import android.content.ContentValues
import android.database.Cursor
import android.net.Uri
import android.util.Log
import java.io.File

class PreloadProvider : ContentProvider() {
    override fun onCreate(): Boolean {
        try {
            // Load packaged lib if present
            val nativeLibDir = context?.applicationInfo?.nativeLibraryDir
            if (!nativeLibDir.isNullOrEmpty()) {
                val packaged = File(nativeLibDir, "librpcsx-android.so")
                if (packaged.exists()) {
                    try {
                        System.load(packaged.absolutePath)
                        Log.i("RPCSX-PreloadProv", "Loaded packaged librpcsx-android.so from $packaged")
                    } catch (e: Throwable) {
                        Log.w("RPCSX-PreloadProv", "System.load(packaged) failed: ${e.message}")
                    }
                }
            }

            // Try library by name
            try {
                System.loadLibrary("rpcsx-android")
                Log.i("RPCSX-PreloadProv", "System.loadLibrary('rpcsx-android') succeeded in provider")
            } catch (e: Throwable) {
                Log.w("RPCSX-PreloadProv", "System.loadLibrary failed in provider: ${e.message}")
            }
        } catch (e: Throwable) {
            Log.w("RPCSX-PreloadProv", "Preload failed: ${e.message}")
        }
        return true
    }

    override fun query(uri: Uri, projection: Array<String>?, selection: String?, selectionArgs: Array<String>?, sortOrder: String?): Cursor? = null
    override fun getType(uri: Uri): String? = null
    override fun insert(uri: Uri, values: ContentValues?): Uri? = null
    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<String>?): Int = 0
    override fun update(uri: Uri, values: ContentValues?, selection: String?, selectionArgs: Array<String>?): Int = 0
}
