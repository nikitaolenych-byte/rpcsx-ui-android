package net.rpcsx.ui.settings

import android.util.Log
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.KeyboardArrowLeft
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import net.rpcsx.RPCSX
import org.json.JSONObject

/**
 * AGVSOL Settings Screen
 * 
 * GPU-specific optimization settings with automatic detection
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AGVSOLSettingsScreen(
    onNavigateBack: () -> Unit
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val scrollState = rememberScrollState()
    
    var isInitialized by remember { mutableStateOf(false) }
    var gpuInfo by remember { mutableStateOf<GPUInfoData?>(null) }
    var activeProfile by remember { mutableStateOf<ProfileData?>(null) }
    var stats by remember { mutableStateOf<AGVSOLStats?>(null) }
    var isLoading by remember { mutableStateOf(true) }
    var errorMessage by remember { mutableStateOf<String?>(null) }
    
    // Settings state
    var targetFPS by remember { mutableIntStateOf(30) }
    var resolutionScale by remember { mutableFloatStateOf(1.0f) }
    
    // Load AGVSOL data
    LaunchedEffect(Unit) {
        withContext(Dispatchers.IO) {
            try {
                isInitialized = RPCSX.instance.agvsolIsInitialized()
                
                if (!isInitialized) {
                    val cacheDir = context.cacheDir.absolutePath
                    isInitialized = RPCSX.instance.agvsolInitialize(cacheDir)
                }
                
                if (isInitialized) {
                    // Parse GPU info
                    val gpuJson = RPCSX.instance.agvsolGetGPUInfo()
                    gpuInfo = parseGPUInfo(gpuJson)
                    
                    // Parse active profile
                    val profileJson = RPCSX.instance.agvsolGetActiveProfile()
                    activeProfile = parseProfile(profileJson)
                    
                    // Parse stats
                    val statsJson = RPCSX.instance.agvsolGetStats()
                    stats = parseStats(statsJson)
                    
                    // Get current settings
                    targetFPS = RPCSX.instance.agvsolGetTargetFPS()
                    resolutionScale = RPCSX.instance.agvsolGetResolutionScale()
                }
                
                isLoading = false
            } catch (e: Exception) {
                Log.e("AGVSOL", "Failed to load AGVSOL data", e)
                errorMessage = e.message
                isLoading = false
            }
        }
    }
    
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("GPU Optimization (AGVSOL)") },
                navigationIcon = {
                    IconButton(onClick = onNavigateBack) {
                        Icon(Icons.Default.KeyboardArrowLeft, "Back")
                    }
                },
                actions = {
                    IconButton(
                        onClick = {
                            scope.launch(Dispatchers.IO) {
                                isLoading = true
                                try {
                                    val cacheDir = context.cacheDir.absolutePath
                                    RPCSX.instance.agvsolShutdown()
                                    RPCSX.instance.agvsolInitialize(cacheDir)
                                    
                                    val gpuJson = RPCSX.instance.agvsolGetGPUInfo()
                                    gpuInfo = parseGPUInfo(gpuJson)
                                    
                                    val profileJson = RPCSX.instance.agvsolGetActiveProfile()
                                    activeProfile = parseProfile(profileJson)
                                    
                                    targetFPS = RPCSX.instance.agvsolGetTargetFPS()
                                    resolutionScale = RPCSX.instance.agvsolGetResolutionScale()
                                } catch (e: Exception) {
                                    Log.e("AGVSOL", "Refresh failed", e)
                                }
                                isLoading = false
                            }
                        }
                    ) {
                        Icon(Icons.Default.Refresh, "Refresh GPU Detection")
                    }
                }
            )
        }
    ) { paddingValues ->
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues)
        ) {
            if (isLoading) {
                CircularProgressIndicator(
                    modifier = Modifier.align(Alignment.Center)
                )
            } else if (errorMessage != null) {
                Column(
                    modifier = Modifier
                        .align(Alignment.Center)
                        .padding(16.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Text(
                        "Error: $errorMessage",
                        color = MaterialTheme.colorScheme.error
                    )
                    Spacer(modifier = Modifier.height(16.dp))
                    Button(
                        onClick = {
                            scope.launch(Dispatchers.IO) {
                                errorMessage = null
                                isLoading = true
                                try {
                                    val cacheDir = context.cacheDir.absolutePath
                                    RPCSX.instance.agvsolInitialize(cacheDir)
                                    isInitialized = true
                                } catch (e: Exception) {
                                    errorMessage = e.message
                                }
                                isLoading = false
                            }
                        }
                    ) {
                        Text("Retry")
                    }
                }
            } else {
                Column(
                    modifier = Modifier
                        .fillMaxSize()
                        .verticalScroll(scrollState)
                        .padding(16.dp)
                ) {
                    // GPU Info Card
                    gpuInfo?.let { gpu ->
                        GPUInfoCard(gpu)
                        Spacer(modifier = Modifier.height(16.dp))
                    }
                    
                    // Active Profile Card
                    activeProfile?.let { profile ->
                        ProfileCard(profile)
                        Spacer(modifier = Modifier.height(16.dp))
                    }
                    
                    // Settings Card
                    SettingsCard(
                        targetFPS = targetFPS,
                        resolutionScale = resolutionScale,
                        onTargetFPSChange = { fps ->
                            targetFPS = fps
                            scope.launch(Dispatchers.IO) {
                                RPCSX.instance.agvsolSetTargetFPS(fps)
                            }
                        },
                        onResolutionScaleChange = { scale ->
                            resolutionScale = scale
                            scope.launch(Dispatchers.IO) {
                                RPCSX.instance.agvsolSetResolutionScale(scale)
                            }
                        }
                    )
                    
                    Spacer(modifier = Modifier.height(16.dp))
                    
                    // Stats Card
                    stats?.let { s ->
                        StatsCard(s)
                    }
                    
                    Spacer(modifier = Modifier.height(32.dp))
                    
                    // Apply Profile Button
                    Button(
                        onClick = {
                            scope.launch(Dispatchers.IO) {
                                RPCSX.instance.agvsolApplyProfile()
                            }
                        },
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text("Apply Optimizations")
                    }
                }
            }
        }
    }
}

@Composable
private fun GPUInfoCard(gpu: GPUInfoData) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(12.dp)
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Text(
                "Detected GPU",
                fontWeight = FontWeight.Bold,
                fontSize = 18.sp
            )
            
            Spacer(modifier = Modifier.height(12.dp))
            
            // Vendor badge
            val vendorColor = when (gpu.vendor.lowercase()) {
                "qualcomm", "adreno" -> Color(0xFF00B0FF)
                "arm", "mali" -> Color(0xFF4CAF50)
                "imagination", "powervr" -> Color(0xFFFF9800)
                else -> MaterialTheme.colorScheme.primary
            }
            
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(
                    modifier = Modifier
                        .background(vendorColor, RoundedCornerShape(4.dp))
                        .padding(horizontal = 8.dp, vertical = 4.dp)
                ) {
                    Text(
                        gpu.vendor,
                        color = Color.White,
                        fontWeight = FontWeight.Bold,
                        fontSize = 12.sp
                    )
                }
                Spacer(modifier = Modifier.width(8.dp))
                Text(gpu.model, fontWeight = FontWeight.Medium)
            }
            
            Spacer(modifier = Modifier.height(8.dp))
            
            // Tier badge
            val tierColor = when (gpu.tier.lowercase()) {
                "ultra" -> Color(0xFF9C27B0)
                "high_end", "high-end" -> Color(0xFF4CAF50)
                "mid_range", "mid-range" -> Color(0xFFFF9800)
                else -> Color(0xFFF44336)
            }
            
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text("Performance Tier: ", color = MaterialTheme.colorScheme.onSurfaceVariant)
                Box(
                    modifier = Modifier
                        .background(tierColor, RoundedCornerShape(4.dp))
                        .padding(horizontal = 8.dp, vertical = 2.dp)
                ) {
                    Text(
                        gpu.tier.uppercase(),
                        color = Color.White,
                        fontSize = 11.sp
                    )
                }
            }
            
            Spacer(modifier = Modifier.height(4.dp))
            
            Text(
                "SoC: ${gpu.soc}",
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                fontSize = 14.sp
            )
            
            if (gpu.estimatedTflops > 0) {
                Text(
                    "Performance: ~${String.format("%.2f", gpu.estimatedTflops)} TFLOPS",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    fontSize = 14.sp
                )
            }
            
            Spacer(modifier = Modifier.height(8.dp))
            
            // Features row
            Row {
                if (gpu.vulkan11) FeatureBadge("Vulkan 1.1")
                if (gpu.vulkan12) FeatureBadge("Vulkan 1.2")
                if (gpu.vulkan13) FeatureBadge("Vulkan 1.3")
                if (gpu.rayTracing) FeatureBadge("Ray Tracing")
            }
        }
    }
}

@Composable
private fun FeatureBadge(text: String) {
    Box(
        modifier = Modifier
            .padding(end = 4.dp)
            .background(MaterialTheme.colorScheme.primaryContainer, RoundedCornerShape(4.dp))
            .padding(horizontal = 6.dp, vertical = 2.dp)
    ) {
        Text(
            text,
            fontSize = 10.sp,
            color = MaterialTheme.colorScheme.onPrimaryContainer
        )
    }
}

@Composable
private fun ProfileCard(profile: ProfileData) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(12.dp)
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Text(
                "Active Profile",
                fontWeight = FontWeight.Bold,
                fontSize = 18.sp
            )
            
            Spacer(modifier = Modifier.height(8.dp))
            
            Text(
                profile.name,
                fontWeight = FontWeight.Medium,
                fontSize = 16.sp,
                color = MaterialTheme.colorScheme.primary
            )
            
            Text(
                profile.description,
                fontSize = 14.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            
            Spacer(modifier = Modifier.height(12.dp))
            
            HorizontalDivider()
            
            Spacer(modifier = Modifier.height(12.dp))
            
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                ProfileStat("Resolution", "${profile.targetWidth}x${profile.targetHeight}")
                ProfileStat("Scale", "${(profile.resolutionScale * 100).toInt()}%")
                ProfileStat("FPS", profile.targetFPS.toString())
            }
        }
    }
}

@Composable
private fun ProfileStat(label: String, value: String) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text(
            value,
            fontWeight = FontWeight.Bold,
            fontSize = 16.sp
        )
        Text(
            label,
            fontSize = 12.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
    }
}

@Composable
private fun SettingsCard(
    targetFPS: Int,
    resolutionScale: Float,
    onTargetFPSChange: (Int) -> Unit,
    onResolutionScaleChange: (Float) -> Unit
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(12.dp)
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Text(
                "Manual Overrides",
                fontWeight = FontWeight.Bold,
                fontSize = 18.sp
            )
            
            Spacer(modifier = Modifier.height(16.dp))
            
            // Target FPS Slider
            Text("Target FPS: $targetFPS")
            Slider(
                value = targetFPS.toFloat(),
                onValueChange = { onTargetFPSChange(it.toInt()) },
                valueRange = 15f..120f,
                steps = 6
            )
            
            Spacer(modifier = Modifier.height(16.dp))
            
            // Resolution Scale Slider
            Text("Resolution Scale: ${(resolutionScale * 100).toInt()}%")
            Slider(
                value = resolutionScale,
                onValueChange = onResolutionScaleChange,
                valueRange = 0.25f..1.5f
            )
        }
    }
}

@Composable
private fun StatsCard(stats: AGVSOLStats) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(12.dp)
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Text(
                "AGVSOL Statistics",
                fontWeight = FontWeight.Bold,
                fontSize = 18.sp
            )
            
            Spacer(modifier = Modifier.height(8.dp))
            
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text("Shaders Loaded:", color = MaterialTheme.colorScheme.onSurfaceVariant)
                Text("${stats.shadersLoaded}")
            }
            
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text("Profiles Available:", color = MaterialTheme.colorScheme.onSurfaceVariant)
                Text("${stats.profilesAvailable}")
            }
            
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Text("Optimized:", color = MaterialTheme.colorScheme.onSurfaceVariant)
                Text(
                    if (stats.isOptimized) "✓ Yes" else "✗ No",
                    color = if (stats.isOptimized) Color(0xFF4CAF50) else Color(0xFFF44336)
                )
            }
        }
    }
}

// Data classes
data class GPUInfoData(
    val vendor: String,
    val model: String,
    val tier: String,
    val soc: String,
    val driverVersion: String,
    val apiVersion: String,
    val vulkan11: Boolean,
    val vulkan12: Boolean,
    val vulkan13: Boolean,
    val computeCapable: Boolean,
    val rayTracing: Boolean,
    val estimatedTflops: Float
)

data class ProfileData(
    val name: String,
    val description: String,
    val resolutionScale: Float,
    val targetWidth: Int,
    val targetHeight: Int,
    val targetFPS: Int,
    val anisotropicLevel: Int,
    val enableBloom: Boolean,
    val useHalfPrecision: Boolean,
    val textureCacheMb: Int,
    val pipelineCache: Boolean
)

data class AGVSOLStats(
    val vendor: Int,
    val tier: Int,
    val profileName: String,
    val shadersLoaded: Int,
    val profilesAvailable: Int,
    val isOptimized: Boolean
)

// JSON Parsing helpers
private fun parseGPUInfo(json: String): GPUInfoData? {
    return try {
        val obj = JSONObject(json)
        GPUInfoData(
            vendor = obj.optString("vendor", "Unknown"),
            model = obj.optString("model", "Unknown"),
            tier = obj.optString("tier", "UNKNOWN"),
            soc = obj.optString("soc", "Unknown"),
            driverVersion = obj.optString("driver_version", ""),
            apiVersion = obj.optString("api_version", ""),
            vulkan11 = obj.optBoolean("vulkan_1_1", false),
            vulkan12 = obj.optBoolean("vulkan_1_2", false),
            vulkan13 = obj.optBoolean("vulkan_1_3", false),
            computeCapable = obj.optBoolean("compute_capable", false),
            rayTracing = obj.optBoolean("ray_tracing", false),
            estimatedTflops = obj.optDouble("estimated_tflops", 0.0).toFloat()
        )
    } catch (e: Exception) {
        Log.e("AGVSOL", "Failed to parse GPU info: $json", e)
        null
    }
}

private fun parseProfile(json: String): ProfileData? {
    return try {
        val obj = JSONObject(json)
        ProfileData(
            name = obj.optString("name", "Default"),
            description = obj.optString("description", ""),
            resolutionScale = obj.optDouble("resolution_scale", 1.0).toFloat(),
            targetWidth = obj.optInt("target_width", 1280),
            targetHeight = obj.optInt("target_height", 720),
            targetFPS = obj.optInt("target_fps", 30),
            anisotropicLevel = obj.optInt("anisotropic_level", 4),
            enableBloom = obj.optBoolean("enable_bloom", true),
            useHalfPrecision = obj.optBoolean("use_half_precision", true),
            textureCacheMb = obj.optInt("texture_cache_mb", 256),
            pipelineCache = obj.optBoolean("pipeline_cache", true)
        )
    } catch (e: Exception) {
        Log.e("AGVSOL", "Failed to parse profile: $json", e)
        null
    }
}

private fun parseStats(json: String): AGVSOLStats? {
    return try {
        val obj = JSONObject(json)
        AGVSOLStats(
            vendor = obj.optInt("vendor", 0),
            tier = obj.optInt("tier", 0),
            profileName = obj.optString("profile", ""),
            shadersLoaded = obj.optInt("shaders_loaded", 0),
            profilesAvailable = obj.optInt("profiles_available", 0),
            isOptimized = obj.optBoolean("optimized", false)
        )
    } catch (e: Exception) {
        Log.e("AGVSOL", "Failed to parse stats: $json", e)
        null
    }
}
