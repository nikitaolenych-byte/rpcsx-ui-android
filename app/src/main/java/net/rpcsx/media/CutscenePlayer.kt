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
            // Better compatibility: prefer Uri/FileDescriptor when possible
            val ctx = CutsceneBridge.getAppContext()
            var dataSet = false
            try {
                if (ctx != null) {
                    val uri = when {
                        path.startsWith("content://") -> android.net.Uri.parse(path)
                        path.startsWith("file://") -> android.net.Uri.parse(path)
                        path.startsWith("http://") || path.startsWith("https://") -> android.net.Uri.parse(path)
                        else -> null
                    }

                    if (uri != null) {
                        try {
                            if (uri.scheme == "content" || uri.scheme == "file") {
                                val afd = ctx.contentResolver.openFileDescriptor(uri, "r")
                                if (afd != null) {
                                    try {
                                        mp.setDataSource(afd.fileDescriptor)
                                        dataSet = true
                                    } finally {
                                        try { afd.close() } catch (_: Throwable) {}
                                    }
                                }
                            } else {
                                mp.setDataSource(ctx, uri)
                                dataSet = true
                            }
                        } catch (_: Throwable) {
                            // fallback to direct path below
                        }
                    }
                }
            } catch (_: Throwable) {}

            if (!dataSet) mp.setDataSource(path)
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
            // As last resort, try launching external player via Intent
            try {
                val ctx = CutsceneBridge.getAppContext()
                if (ctx != null) {
                    val intent = android.content.Intent(android.content.Intent.ACTION_VIEW).apply {
                        setDataAndType(android.net.Uri.parse(path), "video/*")
                        addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK)
                    }
                    ctx.startActivity(intent)
                    return
                }
            } catch (_: Throwable) {}

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

