package net.rpcsx.media

import android.media.MediaPlayer
import android.util.Log
import android.view.Surface
import java.io.IOException
import java.util.concurrent.atomic.AtomicReference

object CutscenePlayer {
    private const val TAG = "CutscenePlayer"
    private val playerRef = AtomicReference<MediaPlayer?>(null)
    private val pendingSurface = AtomicReference<Surface?>(null)

    fun play(path: String, surface: Surface? = null, onComplete: (() -> Unit)? = null) {
        stop()

        val mp = MediaPlayer()
        try {
            val useSurface = surface ?: pendingSurface.get()
            if (useSurface != null) mp.setSurface(useSurface)
            mp.setDataSource(path)
            mp.isLooping = false
            mp.setOnPreparedListener {
                try {
                    it.start()
                } catch (e: Throwable) {
                    Log.e(TAG, "Failed to start MediaPlayer: ${e.message}")
                    onComplete?.invoke()
                }
            }
            mp.setOnCompletionListener {
                onComplete?.invoke()
                stop()
            }
            mp.setOnErrorListener { _, what, extra ->
                Log.e(TAG, "MediaPlayer error what=$what extra=$extra")
                onComplete?.invoke()
                stop()
                true
            }
            mp.prepareAsync()
            playerRef.set(mp)
        } catch (e: IOException) {
            Log.e(TAG, "Failed to set data source: ${e.message}")
            mp.release()
            onComplete?.invoke()
        }
    }

    fun stop() {
        val mp = playerRef.getAndSet(null) ?: return
        try {
            mp.setOnCompletionListener(null)
            mp.setOnPreparedListener(null)
            mp.setOnErrorListener(null)
            if (mp.isPlaying) mp.stop()
        } catch (_: Throwable) {
        }
        try { mp.release() } catch (_: Throwable) {}
    }

    fun setSurface(surface: Surface?) {
        pendingSurface.set(surface)
        val mp = playerRef.get()
        if (mp != null) {
            try {
                mp.setSurface(surface)
            } catch (e: Throwable) {
                Log.w(TAG, "Failed to update MediaPlayer surface", e)
            }
        }
    }
}

