package net.rpcsx

import android.content.Context
import android.util.AttributeSet
import android.view.SurfaceHolder
import android.view.SurfaceView
import kotlin.concurrent.thread
import net.rpcsx.utils.safeNativeCall

class GraphicsFrame : SurfaceView, SurfaceHolder.Callback {
    constructor(context: Context) : super(context) {
        holder.addCallback(this)
    }

    constructor(context: Context?, attrs: AttributeSet?) : super(context, attrs) {
        holder.addCallback(this)
    }

    constructor(context: Context?, attrs: AttributeSet?, defStyleAttr: Int) : super(
        context,
        attrs,
        defStyleAttr
    ) {
        holder.addCallback(this)
    }

    constructor(
        context: Context?,
        attrs: AttributeSet?,
        defStyleAttr: Int,
        defStyleRes: Int
    ) : super(context, attrs, defStyleAttr, defStyleRes) {
        holder.addCallback(this)
    }

    override fun surfaceCreated(p0: SurfaceHolder) {
        // Inform native layer about the surface and also provide it to CutscenePlayer
        safeNativeCall { RPCSX.instance.surfaceEvent(p0.surface, 0) }
        try {
            net.rpcsx.media.CutscenePlayer.setSurface(p0.surface)
        } catch (e: Throwable) {
            android.util.Log.w("GraphicsFrame", "Failed to set cutscene surface", e)
        }
    }

    override fun surfaceChanged(p0: SurfaceHolder, p1: Int, p2: Int, p3: Int) {
        safeNativeCall { RPCSX.instance.surfaceEvent(p0.surface, 1) }
    }

    override fun surfaceDestroyed(p0: SurfaceHolder) {
        safeNativeCall { RPCSX.instance.surfaceEvent(p0.surface, 2) }
        try {
            net.rpcsx.media.CutscenePlayer.setSurface(null)
        } catch (e: Throwable) {
            android.util.Log.w("GraphicsFrame", "Failed to clear cutscene surface", e)
        }
    }
}