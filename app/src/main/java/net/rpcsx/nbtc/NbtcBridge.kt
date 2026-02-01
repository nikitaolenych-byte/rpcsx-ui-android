package net.rpcsx.nbtc

import android.content.Context
import java.io.File

object NbtcBridge {
    init {
        System.loadLibrary("rpcsx-android")
    }

    @JvmStatic
    external fun initialize(modelPath: String?): Boolean

    /**
     * Copy model from APK assets into app files then call native initialize with absolute path.
     * If copying fails, falls back to calling `initialize(null)` which leaves NBTC in prototype mode.
     */
    @JvmStatic
    fun initializeFromAssets(context: Context, assetPath: String = "nbtc/model.tflite"): Boolean {
        return try {
            context.assets.open(assetPath).use { input ->
                val outFile = File(context.filesDir, "nbtc_model.tflite")
                outFile.outputStream().use { output ->
                    input.copyTo(output)
                }
                initialize(outFile.absolutePath)
            }
        } catch (e: Exception) {
            initialize(null)
        }
    }

    @JvmStatic
    external fun analyzeAndCache(gamePath: String): Boolean

    @JvmStatic
    external fun loadCacheForGame(gameId: String): Boolean

    @JvmStatic
    external fun shutdown()
}
