package net.rpcsx.media

import android.net.Uri
import android.os.Bundle
import android.util.Log
import android.view.Surface
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.FrameLayout
import androidx.activity.ComponentActivity

class CutsceneActivity : ComponentActivity(), SurfaceHolder.Callback {
    companion object {
        private var current: CutsceneActivity? = null
        private const val TAG = "CutsceneActivity"

        fun stopIfRunning() {
            current?.runOnUiThread {
                try {
                    current?.finish()
                } catch (e: Throwable) {
                    Log.e(TAG, "Error stopping cutscene: ${e.message}")
                }
            }
        }
    }

    private lateinit var surfaceView: SurfaceView
    private var pendingPath: String? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        current = this

        val path = intent?.getStringExtra("cutscene_path")
        val root = FrameLayout(this)

        surfaceView = SurfaceView(this)
        root.addView(surfaceView, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ))

        setContentView(root)

        if (path == null) {
            finish()
            return
        }

        pendingPath = path
        surfaceView.holder.addCallback(this)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        val path = pendingPath ?: return
        try {
            val filePath = if (path.startsWith("content:") || path.startsWith("file:")) {
                path
            } else {
                "file://" + path
            }

            // Start hardware-accelerated playback via MediaPlayer-backed CutscenePlayer
            CutscenePlayer.play(filePath, holder.surface) {
                finish()
            }
        } catch (e: Throwable) {
            Log.e(TAG, "Failed to play cutscene: ${e.message}")
            finish()
        }
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        try { CutscenePlayer.stop() } catch (_: Throwable) {}
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {}

    override fun onDestroy() {
        try { CutscenePlayer.stop() } catch (_: Throwable) {}
        if (current === this) current = null
        super.onDestroy()
    }
}
