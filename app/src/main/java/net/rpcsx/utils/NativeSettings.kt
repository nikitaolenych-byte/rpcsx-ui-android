package net.rpcsx.utils

import android.util.Log
import net.rpcsx.RPCSX

fun safeSettingsSet(path: String, value: String): Boolean {
    if (RPCSX.activeLibrary.value == null) {
        Log.w("NativeSettings", "Cannot set $path: RPCSX library not loaded")
        return false
    }
    return try {
        val res = RPCSX.instance.settingsSet(path, value)
        if (!res) Log.w("NativeSettings", "settingsSet returned false for $path = $value")
        res
    } catch (e: Throwable) {
        Log.e("NativeSettings", "Error setting $path to $value: ${e.message}", e)
        false
    }
}

/**
 * Apply saved llvm cpu preference to native settings (best-effort).
 */
fun applySavedLlvmCpu() {
    try {
        val pref = GeneralSettings["llvm_cpu_core"] as? String ?: return
        val token = displayToCpuToken(pref)
        safeSettingsSet("Core@@LLVM CPU Core", "\"$token\"")
    } catch (e: Throwable) {
        Log.e("NativeSettings", "Failed to apply saved LLVM CPU: ${e.message}", e)
    }
}
