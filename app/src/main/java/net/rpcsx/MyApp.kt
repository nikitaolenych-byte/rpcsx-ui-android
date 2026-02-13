package net.rpcsx

import android.app.Application
import android.util.Log
import java.io.File

class MyApp : Application() {
    override fun onCreate() {
        super.onCreate()

        try {
            // Try packaged library first
            val nativeLibDir = applicationInfo.nativeLibraryDir
            val packaged = File(nativeLibDir, "librpcsx-android.so")
            if (packaged.exists()) {
                try {
                    System.load(packaged.absolutePath)
                    Log.i("RPCSX-MyApp", "Loaded packaged librpcsx-android.so from $packaged")
                } catch (e: Throwable) {
                    Log.w("RPCSX-MyApp", "System.load(packaged) failed: ${e.message}")
                }
            }

            // Try system load by library name
            try {
                System.loadLibrary("rpcsx-android")
                Log.i("RPCSX-MyApp", "System.loadLibrary('rpcsx-android') succeeded")
            } catch (e: Throwable) {
                Log.w("RPCSX-MyApp", "System.loadLibrary('rpcsx-android') failed: ${e.message}")
            }

        } catch (e: Throwable) {
            Log.w("RPCSX-MyApp", "Preload failed: ${e.message}")
        }
    }
}
