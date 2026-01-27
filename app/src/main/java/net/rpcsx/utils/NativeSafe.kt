package net.rpcsx.utils

import android.util.Log
import net.rpcsx.RPCSX

/**
 * Small helper to safely call into the native `RPCSX.instance` methods.
 * Returns `null` on error or when the native library is not loaded.
 */
inline fun <T> safeNativeCall(block: () -> T?): T? {
    if (RPCSX.activeLibrary.value == null) return null
    return try {
        block()
    } catch (e: Throwable) {
        Log.e("NativeSafe", "Native call failed: ${e.message}", e)
        null
    }
}
