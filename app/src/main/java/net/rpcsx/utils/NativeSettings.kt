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

/**
 * Apply a saved settings snapshot (JSON produced by `settingsGet`) to native settings.
 * This walks the JSON tree and for entries with a "type" property uses the stored "value"
 * and issues `settingsSet` calls for the path formed by joining section names with "@@".
 */
fun applySettingsSnapshot(jsonString: String) {
    try {
        if (RPCSX.activeLibrary.value == null) return
        val json = org.json.JSONObject(jsonString)

        fun recurse(obj: org.json.JSONObject, currentPath: String) {
            val keys = obj.keys()
            while (keys.hasNext()) {
                val key = keys.next()
                val v = obj.opt(key)
                if (v is org.json.JSONObject) {
                    // leaf if it has 'type' and 'value'
                    if (v.has("type") && v.has("value")) {
                        val value = v.opt("value")?.toString() ?: continue
                        val fullPath = if (currentPath.isEmpty()) key else currentPath + "@@" + key
                        safeSettingsSet(fullPath, value.toString())
                    } else {
                        val nextPath = if (currentPath.isEmpty()) key else currentPath + "@@" + key
                        recurse(v, nextPath)
                    }
                }
            }
        }

        recurse(json, "")
    } catch (e: Throwable) {
        android.util.Log.e("NativeSettings", "Failed to apply settings snapshot: ${e.message}", e)
    }
}

/**
 * Convenience: apply saved per-game config stored in GeneralSettings under key `game_config::<path>`
 */
fun applySavedGameConfig(gamePath: String) {
    try {
        val key = "game_config::${gamePath}"
        val saved = GeneralSettings[key] as? String ?: return
        applySettingsSnapshot(saved)
    } catch (e: Throwable) {
        android.util.Log.e("NativeSettings", "Failed to apply saved game config: ${e.message}", e)
    }
}
