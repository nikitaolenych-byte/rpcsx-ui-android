package net.rpcsx.utils

import android.util.Log
import net.rpcsx.RPCSX

// Helper to restore PPU/SPU decoder settings to known-working values
object DecoderUtils {
    private fun safeSettingsSet(path: String, value: String): Boolean {
        if (RPCSX.activeLibrary.value == null) return false
        return try {
            val result = RPCSX.instance.settingsSet(path, value)
            if (!result) Log.w("DecoderUtils", "settingsSet returned false for $path = $value")
            result
        } catch (e: Throwable) {
            Log.e("DecoderUtils", "Error setting $path to $value: ${e.message}")
            false
        }
    }

    fun restoreDecodersToSafeDefaults(): Boolean {
        if (RPCSX.activeLibrary.value == null) return false
        var ok = true
        ok = ok && safeSettingsSet("Core@@PPU Decoder", "\"LLVM Recompiler (Legacy)\"")
        ok = ok && safeSettingsSet("Core@@SPU Decoder", "\"LLVM Recompiler (Legacy)\"")
        ok = ok && safeSettingsSet("Core@@PPU LLVM Version", "\"19\"")
        ok = ok && safeSettingsSet("Core@@SPU LLVM Version", "\"19\"")
        Log.i("DecoderUtils", "restoreDecodersToSafeDefaults: success=$ok")
        return ok
    }
}
