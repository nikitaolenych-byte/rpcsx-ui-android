package net.rpcsx.ui.settings

import android.widget.Toast
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowLeft
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LargeTopAppBar
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay
import net.rpcsx.R
import net.rpcsx.RPCSX
import net.rpcsx.ui.settings.components.core.PreferenceHeader
import net.rpcsx.ui.settings.components.core.PreferenceIcon
import net.rpcsx.ui.settings.components.core.PreferenceValue
import net.rpcsx.ui.settings.components.preference.RegularPreference
import net.rpcsx.ui.settings.components.preference.SliderPreference
import net.rpcsx.ui.settings.components.preference.SwitchPreference
import net.rpcsx.utils.GeneralSettings

/**
 * RSX Video Settings Screen
 * Configure RSX Graphics Engine (multithreaded) settings
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun RSXVideoSettingsScreen(
    modifier: Modifier = Modifier,
    navigateBack: () -> Unit
) {
    val context = LocalContext.current
    val topBarScrollBehavior = TopAppBarDefaults.enterAlwaysScrollBehavior()
    
    // RSX state
    var rsxEnabled by remember { mutableStateOf(RPCSX.instance.rsxIsRunning()) }
    var threadCount by remember { mutableIntStateOf(RPCSX.instance.rsxGetThreadCount()) }
    var currentFPS by remember { mutableIntStateOf(0) }
    var rsxStats by remember { mutableStateOf("") }
    
    // Auto-update FPS every 500ms
    LaunchedEffect(rsxEnabled) {
        while (rsxEnabled) {
            currentFPS = RPCSX.instance.getRSXFPS()
            rsxStats = RPCSX.instance.rsxGetStats()
            delay(500)
        }
    }
    
    Scaffold(
        modifier = Modifier
            .nestedScroll(topBarScrollBehavior.nestedScrollConnection)
            .then(modifier),
        topBar = {
            LargeTopAppBar(
                title = {
                    Text(
                        text = stringResource(R.string.rsx_video_settings),
                        fontWeight = FontWeight.Medium
                    )
                },
                scrollBehavior = topBarScrollBehavior,
                navigationIcon = {
                    IconButton(onClick = navigateBack) {
                        Icon(
                            imageVector = Icons.AutoMirrored.Default.KeyboardArrowLeft,
                            contentDescription = null
                        )
                    }
                }
            )
        }
    ) { contentPadding ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(contentPadding)
        ) {
            // RSX Engine Toggle
            item(key = "rsx_enabled") {
                SwitchPreference(
                    checked = rsxEnabled,
                    title = stringResource(R.string.rsx_enabled),
                    description = stringResource(R.string.rsx_enabled_description),
                    leadingIcon = {
                        PreferenceIcon(icon = painterResource(R.drawable.ic_rsx_engine))
                    },
                    onClick = { enabled ->
                        if (enabled) {
                            val success = RPCSX.instance.rsxStart()
                            if (success) {
                                rsxEnabled = true
                                Toast.makeText(context, context.getString(R.string.rsx_status_on), Toast.LENGTH_SHORT).show()
                            }
                        } else {
                            RPCSX.instance.rsxStop()
                            rsxEnabled = false
                            Toast.makeText(context, context.getString(R.string.rsx_status_off), Toast.LENGTH_SHORT).show()
                        }
                        // Save preference
                        GeneralSettings.setValue("rsx_enabled", enabled)
                    }
                )
            }
            
            // Thread Count Slider
            item(key = "rsx_thread_count") {
                SliderPreference(
                    value = threadCount.toFloat(),
                    valueRange = 1f..8f,
                    title = stringResource(R.string.rsx_thread_count),
                    description = stringResource(R.string.rsx_thread_count_description),
                    steps = 6, // 1-8 = 7 positions, 6 intermediate steps
                    onValueChange = { value ->
                        val count = value.toInt()
                        RPCSX.instance.rsxSetThreadCount(count)
                        threadCount = count
                        GeneralSettings.setValue("rsx_thread_count", count)
                    },
                    valueContent = {
                        PreferenceValue(text = "$threadCount threads")
                    }
                )
            }
            
            // FPS Counter (read-only)
            item(key = "rsx_fps") {
                RegularPreference(
                    title = stringResource(R.string.rsx_stats),
                    description = if (rsxEnabled) {
                        "FPS: $currentFPS | Threads: $threadCount"
                    } else {
                        stringResource(R.string.rsx_status_off)
                    },
                    leadingIcon = null,
                    onClick = {
                        // Show detailed stats on click
                        if (rsxStats.isNotEmpty()) {
                            Toast.makeText(context, rsxStats, Toast.LENGTH_LONG).show()
                        }
                    }
                )
            }
            
            // Spacer
            item {
                Spacer(modifier = Modifier.height(16.dp))
            }
            
            // Header for advanced options
            item(key = "header_advanced") {
                PreferenceHeader(title = "Performance")
            }
            
            // Resolution Scale (future feature placeholder)
            item(key = "resolution_scale") {
                SliderPreference(
                    value = (GeneralSettings["rsx_resolution_scale"] as? Int ?: 100).toFloat(),
                    valueRange = 50f..200f,
                    title = "Resolution Scale",
                    description = "Internal rendering resolution (50%-200%)",
                    steps = 5, // 50, 75, 100, 125, 150, 175, 200
                    onValueChange = { value ->
                        val scale = value.toInt()
                        GeneralSettings.setValue("rsx_resolution_scale", scale)
                    },
                    valueContent = {
                        PreferenceValue(text = "${(GeneralSettings["rsx_resolution_scale"] as? Int ?: 100)}%")
                    }
                )
            }
            
            // VSync option
            item(key = "vsync") {
                var vsyncEnabled by remember { 
                    mutableStateOf(GeneralSettings["rsx_vsync"] as? Boolean ?: true) 
                }
                SwitchPreference(
                    checked = vsyncEnabled,
                    title = "VSync",
                    description = "Synchronize frame rate with display refresh",
                    leadingIcon = null,
                    onClick = { enabled ->
                        vsyncEnabled = enabled
                        GeneralSettings.setValue("rsx_vsync", enabled)
                    }
                )
            }
            
            // Frame Limit option
            item(key = "frame_limit") {
                var frameLimitEnabled by remember { 
                    mutableStateOf(GeneralSettings["rsx_frame_limit"] as? Boolean ?: true) 
                }
                SwitchPreference(
                    checked = frameLimitEnabled,
                    title = "Frame Limit",
                    description = "Limit frame rate to 60 FPS",
                    leadingIcon = null,
                    onClick = { enabled ->
                        frameLimitEnabled = enabled
                        GeneralSettings.setValue("rsx_frame_limit", enabled)
                    }
                )
            }
        }
    }
}
