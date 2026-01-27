package net.rpcsx.utils

import android.util.Log
import net.rpcsx.RPCSX
import net.rpcsx.utils.safeSettingsSet

// Helper to restore PPU/SPU decoder settings to known-working values
object DecoderUtils {
    // Use shared safeSettingsSet from net.rpcsx.utils

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
