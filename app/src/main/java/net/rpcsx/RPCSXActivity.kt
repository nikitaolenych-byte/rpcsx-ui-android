package net.rpcsx

import android.app.Activity
import android.os.Bundle
import android.util.Log
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.ViewGroup.MarginLayoutParams
import android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.core.view.isInvisible
import androidx.core.view.updateLayoutParams
import net.rpcsx.databinding.ActivityRpcs3Binding
import net.rpcsx.dialogs.AlertDialogQueue
import net.rpcsx.overlay.State
import net.rpcsx.utils.GeneralSettings
import net.rpcsx.utils.InputBindingPrefs
import kotlin.concurrent.thread
import kotlin.math.abs

class RPCSXActivity : Activity() {
    private lateinit var binding: ActivityRpcs3Binding
    private lateinit var unregisterUsbEventListener: () -> Unit
    private var gamePadState: State = State()
    private var usesAxisL2 = false
    private var usesAxisR2 = false
    private var bootThread: Thread? = null
    private val inputBindings by lazy { InputBindingPrefs.loadBindings() }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityRpcs3Binding.inflate(layoutInflater)
        setContentView(binding.root)

        unregisterUsbEventListener = listenUsbEvents(this)
        enableFullScreenImmersive()

        binding.oscToggle.setOnClickListener {
            binding.padOverlay.isInvisible = !binding.padOverlay.isInvisible
            binding.oscToggle.setImageResource(if (binding.padOverlay.isInvisible) R.drawable.ic_osc_off else R.drawable.ic_show_osc)
        }

        // RSX Graphics Engine Toggle Button
        setupRSXButton()

        val gamePath = intent.getStringExtra("path")!!
        RPCSX.lastPlayedGame = gamePath
        
        // Restore NCE mode from saved preferences before boot
        restoreNCEMode()
        
        // Start FPS counter update
        startFPSCounter()

        bootThread = thread {
            if (RPCSX.getState() != EmulatorState.Stopped) {
                val state = RPCSX.getState()
                Log.w("RPCSX State", state.name)

                if (state == EmulatorState.Paused && RPCSX.activeGame.value == gamePath) {
                    RPCSX.instance.resume()
                    return@thread
                }

                if (RPCSX.getState() != EmulatorState.Stopping && RPCSX.getState() != EmulatorState.Stopped) {
                    RPCSX.instance.kill()

                    while (RPCSX.getState() != EmulatorState.Stopped) {
                        Thread.sleep(300)
                        if (Thread.interrupted()) {
                            return@thread
                        }
                    }
                }
            }

            Log.w("RPCSX State", RPCSX.getState().name)
            RPCSX.activeGame.value = gamePath

            val bootResult = RPCSX.boot(gamePath)
            if (bootResult != BootResult.NoErrors) {
                AlertDialogQueue.showDialog(
                    getString(R.string.failed_to_boot),
                    getString(R.string.error_with_msg, bootResult.name)
                )
                finish()
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        RPCSX.state.value = EmulatorState.Paused
        unregisterUsbEventListener()
        bootThread?.interrupt()
        bootThread?.join()
    }

    /**
     * Restore NCE mode from saved preferences before game boot.
     * This prevents NCE from being reset to interpreter when launching games.
     */
    private fun restoreNCEMode() {
        val savedMode = GeneralSettings.nceMode
        if (savedMode >= 0) {
            try {
                RPCSX.instance.setNCEMode(savedMode)
                Log.i("RPCSX", "Restored NCE mode before boot: $savedMode (${
                    when (savedMode) {
                        0 -> "Disabled"
                        1 -> "Interpreter"
                        2 -> "Recompiler"
                        3 -> "NCE/JIT"
                        else -> "Unknown"
                    }
                })")
            } catch (e: Throwable) {
                Log.e("RPCSX", "Failed to restore NCE mode", e)
            }
        } else {
            // Default to NCE/JIT if not set
            try {
                RPCSX.instance.setNCEMode(3)
                GeneralSettings.nceMode = 3
                Log.i("RPCSX", "NCE mode not set, defaulting to NCE/JIT (mode 3)")
            } catch (e: Throwable) {
                Log.e("RPCSX", "Failed to set default NCE mode", e)
            }
        }
    }

    private fun keyCodeToPadBit(keyCode: Int): Pair<Int, Int> {
        val event = inputBindings[keyCode] ?: Pair(0, 0)
        
        if (keyCode == KeyEvent.KEYCODE_BUTTON_R2) {
            if (usesAxisR2) return Pair(0, 0) else return event
        }
        
        if (keyCode == KeyEvent.KEYCODE_BUTTON_L2) {
            if (usesAxisL2) return Pair(0, 0) else return event
        }
        
        return event
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        if (event == null || (event.source and (InputDevice.SOURCE_GAMEPAD or InputDevice.SOURCE_JOYSTICK or InputDevice.SOURCE_DPAD)) == 0 || event.repeatCount != 0) {
            return super.onKeyDown(keyCode, event)
        }
        val padBit = keyCodeToPadBit(keyCode)
        if (padBit.first == 0) {
            return super.onKeyDown(keyCode, event)
        }

        gamePadState.digital[padBit.second] = gamePadState.digital[padBit.second] or padBit.first
        sendGamepadData()
        return true
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        if (event == null || event.source and (InputDevice.SOURCE_GAMEPAD or InputDevice.SOURCE_JOYSTICK or InputDevice.SOURCE_DPAD) == 0) {
            return super.onKeyUp(keyCode, event)
        }

        val padBit = keyCodeToPadBit(keyCode)
        if (padBit.first == 0) {
            return super.onKeyUp(keyCode, event)
        }

        gamePadState.digital[padBit.second] =
            gamePadState.digital[padBit.second] and padBit.first.inv()
        sendGamepadData()
        return true
    }

    override fun onGenericMotionEvent(event: MotionEvent?): Boolean {
        if (event == null || event.source and InputDevice.SOURCE_JOYSTICK != InputDevice.SOURCE_JOYSTICK || event.action != MotionEvent.ACTION_MOVE) {
            return super.onGenericMotionEvent(event)
        }

        if (event.getAxisValue(MotionEvent.AXIS_LTRIGGER) > 0.1) {
            gamePadState.digital[1] =
                gamePadState.digital[1] or Digital2Flags.CELL_PAD_CTRL_L2.bit
            usesAxisL2 = true
        } else if (usesAxisL2) {
            usesAxisL2 = false
            gamePadState.digital[1] =
                gamePadState.digital[1] and Digital2Flags.CELL_PAD_CTRL_L2.bit.inv()
        }

        if (event.getAxisValue(MotionEvent.AXIS_RTRIGGER) > 0.1) {
            gamePadState.digital[1] =
                gamePadState.digital[1] or Digital2Flags.CELL_PAD_CTRL_R2.bit
            usesAxisR2 = true
        } else if (usesAxisR2) {
            usesAxisR2 = false
            gamePadState.digital[1] =
                gamePadState.digital[1] and Digital2Flags.CELL_PAD_CTRL_R2.bit.inv()
        }

        val dpadX = event.getAxisValue(MotionEvent.AXIS_HAT_X)
        val dpadY = event.getAxisValue(MotionEvent.AXIS_HAT_Y)

        gamePadState.digital[0] =
            gamePadState.digital[0] and (Digital1Flags.CELL_PAD_CTRL_LEFT.bit or Digital1Flags.CELL_PAD_CTRL_RIGHT.bit or Digital1Flags.CELL_PAD_CTRL_UP.bit or Digital1Flags.CELL_PAD_CTRL_DOWN.bit).inv()
        if (abs(dpadX) > 0.1f) {
            if (dpadX < 0) {
                gamePadState.digital[0] =
                    gamePadState.digital[0] or Digital1Flags.CELL_PAD_CTRL_LEFT.bit
            } else {
                gamePadState.digital[0] =
                    gamePadState.digital[0] or Digital1Flags.CELL_PAD_CTRL_RIGHT.bit
            }
        }

        if (abs(dpadY) > 0.1f) {
            if (dpadY < 0) {
                gamePadState.digital[0] =
                    gamePadState.digital[0] or Digital1Flags.CELL_PAD_CTRL_UP.bit
            } else {
                gamePadState.digital[0] =
                    gamePadState.digital[0] or Digital1Flags.CELL_PAD_CTRL_DOWN.bit
            }
        }

        gamePadState.leftStickX = (event.getAxisValue(MotionEvent.AXIS_X) * 127 + 128).toInt()
        gamePadState.leftStickY = (event.getAxisValue(MotionEvent.AXIS_Y) * 127 + 128).toInt()
        gamePadState.rightStickX = (event.getAxisValue(MotionEvent.AXIS_Z) * 127 + 128).toInt()
        gamePadState.rightStickY = (event.getAxisValue(MotionEvent.AXIS_RZ) * 127 + 128).toInt()

        sendGamepadData()
        return true
    }

    private fun sendGamepadData() {
        RPCSX.instance.overlayPadData(
            gamePadState.digital[0],
            gamePadState.digital[1],
            gamePadState.leftStickX,
            gamePadState.leftStickY,
            gamePadState.rightStickX,
            gamePadState.rightStickY
        )
    }

    private fun enableFullScreenImmersive() {
        with(window) {
            WindowCompat.setDecorFitsSystemWindows(this, false)
            val insetsController = WindowInsetsControllerCompat(this, decorView)
            insetsController.apply {
                hide(WindowInsetsCompat.Type.systemBars())
                systemBarsBehavior =
                    WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
            attributes.layoutInDisplayCutoutMode = LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        }
        applyInsetsToPadOverlay()
    }

    private fun applyInsetsToPadOverlay() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.padOverlay) { view, windowInsets ->
            // I don't think we need `displayCutout` insets here as well
            // Since there is hardly any overlay overlapping with it
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            view.updateLayoutParams<MarginLayoutParams> {
                leftMargin = insets.left
                rightMargin = insets.right
                topMargin = insets.top
                bottomMargin = insets.bottom
            }
            WindowInsetsCompat.CONSUMED
        }
    }

    // ============================================================================
    // RSX Graphics Engine Controls
    // ============================================================================
    
    private var rsxEnabled = false
    private var fpsUpdateHandler: android.os.Handler? = null
    private var fpsUpdateRunnable: Runnable? = null
    
    /**
     * Setup RSX toggle button and its functionality
     */
    private fun setupRSXButton() {
        binding.rsxToggle.setOnClickListener {
            rsxEnabled = !rsxEnabled
            
            if (rsxEnabled) {
                // Start RSX Graphics Engine
                val result = try { RPCSX.instance.rsxStart() } catch (e: Throwable) { false }
                if (result) {
                    binding.rsxToggle.isSelected = true
                    binding.rsxStatus.text = "RSX: ON"
                    binding.rsxStatus.setTextColor(android.graphics.Color.parseColor("#00FF00"))
                    Log.i("RPCSX-RSX", "RSX Graphics Engine started (multithreaded)")
                } else {
                    rsxEnabled = false
                    Log.e("RPCSX-RSX", "Failed to start RSX Graphics Engine")
                }
            } else {
                // Stop RSX Graphics Engine
                try {
                    RPCSX.instance.rsxStop()
                } catch (e: Throwable) {
                    Log.e("RPCSX-RSX", "Failed to stop RSX Graphics Engine", e)
                }
                binding.rsxToggle.isSelected = false
                binding.rsxStatus.text = "RSX: OFF"
                binding.rsxStatus.setTextColor(android.graphics.Color.parseColor("#80FFFFFF"))
                Log.i("RPCSX-RSX", "RSX Graphics Engine stopped")
            }
        }
        
        // Long press for RSX stats
        binding.rsxToggle.setOnLongClickListener {
            showRSXStats()
            true
        }
    }
    
    /**
     * Start FPS counter update loop
     */
    private fun startFPSCounter() {
        fpsUpdateHandler = android.os.Handler(mainLooper)
        fpsUpdateRunnable = object : Runnable {
            override fun run() {
                if (rsxEnabled) {
                    val fps = try { RPCSX.instance.getRSXFPS() } catch (e: Throwable) { 0 }
                    binding.fpsCounter.text = "FPS: $fps"
                    
                    // Color based on FPS
                    val color = when {
                        fps >= 55 -> android.graphics.Color.parseColor("#00FF00")  // Green
                        fps >= 30 -> android.graphics.Color.parseColor("#FFFF00")  // Yellow
                        else -> android.graphics.Color.parseColor("#FF0000")       // Red
                    }
                    binding.fpsCounter.setTextColor(color)
                } else {
                    binding.fpsCounter.text = "FPS: --"
                    binding.fpsCounter.setTextColor(android.graphics.Color.parseColor("#80FFFFFF"))
                }
                fpsUpdateHandler?.postDelayed(this, 500)
            }
        }
        fpsUpdateHandler?.post(fpsUpdateRunnable!!)
    }
    
    /**
     * Show RSX Graphics Engine statistics
     */
    private fun showRSXStats() {
        val stats = try { RPCSX.instance.rsxGetStats() } catch (e: Throwable) { "N/A" }
        val message = """
            |RSX Graphics Engine Stats:
            |─────────────────────────
            |$stats
            |Worker Threads: 4
            |Backend: Vulkan 1.3
        """.trimMargin()
        
        android.widget.Toast.makeText(this, message, android.widget.Toast.LENGTH_LONG).show()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) enableFullScreenImmersive()
    }
}
