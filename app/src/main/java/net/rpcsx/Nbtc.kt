package net.rpcsx

object Nbtc {
    init {
        // native library 'rpcsx-android' already loaded by app; ensure it's available
    }

    @JvmStatic
    external fun init(cacheDir: String, modelPath: String?): Boolean

    @JvmStatic
    external fun analyzeAndCache(gamePath: String): Boolean

    @JvmStatic
    external fun loadCache(gamePath: String): Boolean
}
