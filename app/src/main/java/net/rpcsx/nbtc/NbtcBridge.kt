package net.rpcsx.nbtc

object NbtcBridge {
    init {
        System.loadLibrary("rpcsx-android")
    }

    @JvmStatic
    external fun initialize(modelPath: String?): Boolean

    @JvmStatic
    external fun analyzeAndCache(gamePath: String): Boolean

    @JvmStatic
    external fun loadCacheForGame(gameId: String): Boolean

    @JvmStatic
    external fun shutdown()
}
