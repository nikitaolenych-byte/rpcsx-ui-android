
package net.rpcsx.utils

import android.content.Context
import android.content.SharedPreferences

object GeneralSettings {

    private lateinit var prefs: SharedPreferences
    
    // In-memory cache for frequently accessed values to avoid SharedPreferences overhead
    private var cachedNceMode: Int? = null

    fun init(context: Context) {
        prefs = context.getSharedPreferences("app_prefs", Context.MODE_PRIVATE)
        // Pre-load cached values
        cachedNceMode = prefs.getInt("nce_mode", -1).takeIf { prefs.contains("nce_mode") }
    }
    
    // Fast access to NCE mode without SharedPreferences lookup
    var nceMode: Int
        get() = cachedNceMode ?: -1
        set(value) {
            cachedNceMode = value
            prefs.edit().putInt("nce_mode", value).apply()
        }

    operator fun get(key: String): Any? = with(prefs) {
        when {
            contains(key) -> {
                all[key]
            }
            else -> null
        }
    }

    fun setValue(key: String, value: Any?) {
        with(prefs.edit()) {
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
        prefs.edit().commit()
    }
}
