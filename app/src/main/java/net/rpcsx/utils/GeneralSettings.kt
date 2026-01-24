
package net.rpcsx.utils

import android.content.Context
import android.content.SharedPreferences
import android.util.Log

object GeneralSettings {

    private var prefs: SharedPreferences? = null
    
    // In-memory cache for frequently accessed values to avoid SharedPreferences overhead
    private var cachedNceMode: Int? = null
    
    val isInitialized: Boolean
        get() = prefs != null

    fun init(context: Context) {
        try {
            prefs = context.getSharedPreferences("app_prefs", Context.MODE_PRIVATE)
            // Pre-load cached values
            prefs?.let { p ->
                cachedNceMode = p.getInt("nce_mode", -1).takeIf { p.contains("nce_mode") }
            }
        } catch (e: Throwable) {
            Log.e("GeneralSettings", "Failed to initialize SharedPreferences", e)
        }
    }
    
    // Fast access to NCE mode without SharedPreferences lookup
    var nceMode: Int
        get() = cachedNceMode ?: -1
        set(value) {
            cachedNceMode = value
            try {
                prefs?.edit()?.putInt("nce_mode", value)?.apply()
            } catch (e: Throwable) {
                Log.e("GeneralSettings", "Failed to save nceMode", e)
            }
        }

    operator fun get(key: String): Any? {
        val p = prefs ?: return null
        return try {
            if (p.contains(key)) p.all[key] else null
        } catch (e: Throwable) {
            Log.e("GeneralSettings", "Failed to get $key", e)
            null
        }
    }

    fun setValue(key: String, value: Any?) {
        val p = prefs ?: return
        try {
            with(p.edit()) {
                when (value) {
                    null -> remove(key)
                    is String -> putString(key, value)
                    is Int -> putInt(key, value)
                    is Boolean -> putBoolean(key, value)
                    is Float -> putFloat(key, value)
                    is Long -> putLong(key, value)
                    else -> throw IllegalArgumentException("Unsupported type: ${value::class.java.name}")
                }
                apply()
            }
        } catch (e: Throwable) {
            Log.e("GeneralSettings", "Failed to set value for $key", e)
        }
    }

    fun Any?.boolean(def: Boolean = false): Boolean {
        return this as? Boolean ?: def
    }

    fun Any?.string(def: String = ""): String {
        return this as? String ?: def
    }

    fun Any?.int(def: Int = 0): Int {
        return this as? Int ?: def
    }

    fun Any?.long(def: Long = 0L): Long {
        return this as? Long ?: def
    }

    fun Any?.float(def: Float = 0f): Float {
        return this as? Float ?: def
    }

    operator fun set(key: String, value: Any?) {
        setValue(key, value)
    }

    fun sync() {
        try {
            prefs?.edit()?.commit()
        } catch (e: Throwable) {
            Log.e("GeneralSettings", "Failed to sync", e)
        }
    }
}
