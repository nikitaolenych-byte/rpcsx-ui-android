package net.rpcsx.ui.settings

import android.util.Log
import android.widget.Toast
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.KeyboardArrowLeft
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LargeTopAppBar
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import net.rpcsx.R
import net.rpcsx.RPCSX
import net.rpcsx.utils.safeNativeCall
import net.rpcsx.ui.settings.components.core.PreferenceHeader
import net.rpcsx.ui.settings.components.preference.RegularPreference
import net.rpcsx.ui.settings.components.preference.SliderPreference
import net.rpcsx.ui.settings.components.preference.SwitchPreference
import net.rpcsx.utils.GeneralSettings

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun RSXSettingsScreen(
    navigateBack: () -> Unit
) {
    val context = LocalContext.current
    val topBarScrollBehavior = TopAppBarDefaults.enterAlwaysScrollBehavior()
    
    Scaffold(
        modifier = Modifier.nestedScroll(topBarScrollBehavior.nestedScrollConnection),
        topBar = {
            LargeTopAppBar(
                title = { Text(stringResource(R.string.rsx_video_settings)) },
                navigationIcon = {
                    IconButton(onClick = navigateBack) {
                        Icon(
                            imageVector = Icons.Filled.KeyboardArrowLeft,
                            contentDescription = "Back"
                        )
                    }
                },
                scrollBehavior = topBarScrollBehavior
            )
        }
    ) { innerPadding ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding)
        ) {
            item(key = "rsx_header") {
                PreferenceHeader(text = "RSX Graphics Engine")
            }
            
            item(key = "rsx_enabled") {
                var rsxEnabled by remember {
                    mutableStateOf(
                        net.rpcsx.utils.safeNativeCall { RPCSX.instance.rsxIsRunning() } ?: false
                    )
                }
                SwitchPreference(
                    checked = rsxEnabled,
                    title = stringResource(R.string.rsx_enabled),
                    subtitle = { Text(stringResource(R.string.rsx_enabled_description)) },
                    onClick = { enabled ->
                        try {
                            if (enabled) {
                                val success = net.rpcsx.utils.safeNativeCall { RPCSX.instance.rsxStart() } ?: false
                                if (success) {
                                    rsxEnabled = true
                                    Toast.makeText(context, context.getString(R.string.rsx_status_on), Toast.LENGTH_SHORT).show()
                                }
                            } else {
                                net.rpcsx.utils.safeNativeCall { RPCSX.instance.rsxStop() }
                                rsxEnabled = false
                                Toast.makeText(context, context.getString(R.string.rsx_status_off), Toast.LENGTH_SHORT).show()
                            }
                            GeneralSettings.setValue("rsx_enabled", enabled)
                        } catch (e: Throwable) {
                            Toast.makeText(context, "RSX not available: ${e.message}", Toast.LENGTH_SHORT).show()
                        }
                    }
                )
            }
            
            item(key = "rsx_thread_count") {
                var threadCount by remember {
                    mutableStateOf(
                        (net.rpcsx.utils.safeNativeCall { RPCSX.instance.rsxGetThreadCount() }?.toFloat()) ?: 8f
                    )
                }
                SliderPreference(
                    value = threadCount,
                    valueRange = 1f..8f,
                    title = stringResource(R.string.rsx_thread_count),
                    steps = 6,
                    onValueChange = { value ->
                        try {
                            val count = value.toInt()
                            net.rpcsx.utils.safeNativeCall { RPCSX.instance.rsxSetThreadCount(count) }
                            threadCount = value
                            GeneralSettings.setValue("rsx_thread_count", count)
                        } catch (e: Throwable) {
                            Log.e("RSX", "Failed to set thread count: ${e.message}")
                        }
                    },
                    valueContent = {
                        Text(text = "${threadCount.toInt()} threads")
                    }
                )
            }
            
            item(key = "rsx_fps_stats") {
                val rsxRunning = net.rpcsx.utils.safeNativeCall { RPCSX.instance.rsxIsRunning() } ?: false
                val fps = if (rsxRunning) { net.rpcsx.utils.safeNativeCall { RPCSX.instance.getRSXFPS() } ?: 0 } else 0
                RegularPreference(
                    title = stringResource(R.string.rsx_stats),
                    subtitle = { Text(if (rsxRunning) "FPS: $fps" else stringResource(R.string.rsx_status_off)) },
                    onClick = {
                        try {
                            val stats = net.rpcsx.utils.safeNativeCall { RPCSX.instance.rsxGetStats() } ?: "RSX stats not available"
                            Toast.makeText(context, stats, Toast.LENGTH_LONG).show()
                        } catch (e: Throwable) {
                            Toast.makeText(context, "RSX stats not available", Toast.LENGTH_SHORT).show()
                        }
                    }
                )
            }
            
            item(key = "rsx_header_quality") {
                PreferenceHeader(text = "Quality Settings")
            }
            
            item(key = "rsx_resolution_scale") {
                var scale by remember { mutableStateOf((GeneralSettings["rsx_resolution_scale"] as? Int ?: 50).toFloat()) }
                SliderPreference(
                    value = scale,
                    valueRange = 50f..200f,
                    title = "Resolution Scale",
                    steps = 5,
                    onValueChange = { value ->
                        scale = value
                        GeneralSettings.setValue("rsx_resolution_scale", value.toInt())
                    },
                    valueContent = {
                        Text(text = "${scale.toInt()}%")
                    }
                )
            }
            
            item(key = "rsx_header_performance") {
                PreferenceHeader(text = "Performance Settings")
            }
            
            item(key = "rsx_vsync") {
                var vsyncEnabled by remember { mutableStateOf(GeneralSettings["rsx_vsync"] as? Boolean ?: false) }
                SwitchPreference(
                    checked = vsyncEnabled,
                    title = "VSync",
                    subtitle = { Text("Synchronize frame rate with display refresh") },
                    onClick = { enabled ->
                        vsyncEnabled = enabled
                        GeneralSettings.setValue("rsx_vsync", enabled)
                    }
                )
            }
            
            item(key = "rsx_frame_limit") {
                var frameLimitEnabled by remember { mutableStateOf(GeneralSettings["rsx_frame_limit"] as? Boolean ?: false) }
                SwitchPreference(
                    checked = frameLimitEnabled,
                    title = "Frame Limit",
                    subtitle = { Text("Limit frame rate to 60 FPS") },
                    onClick = { enabled ->
                        frameLimitEnabled = enabled
                        GeneralSettings.setValue("rsx_frame_limit", enabled)
                    }
                )
            }
        }
    }
}
