package net.rpcsx.media

import android.content.Context
import android.content.Intent
import android.util.Log

object CutsceneBridge {
    private const val TAG = "CutsceneBridge"
    private var appContext: Context? = null

    @JvmStatic
    fun init(context: Context) {
        appContext = context.applicationContext
        Log.i(TAG, "Initialized CutsceneBridge with application context")
    }

    // Expose application context for compatibility helpers
    @JvmStatic
    fun getAppContext(): Context? = appContext

    @JvmStatic
    fun playFromNative(path: String) {
        val ctx = appContext ?: run {
            Log.e(TAG, "playFromNative called before init")
            return
        }

        val intent = Intent(ctx, CutsceneActivity::class.java).apply {
            putExtra("cutscene_path", path)
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        }
        ctx.startActivity(intent)
    }

    @JvmStatic
    fun stopFromNative() {
        CutsceneActivity.stopIfRunning()
    }
}

