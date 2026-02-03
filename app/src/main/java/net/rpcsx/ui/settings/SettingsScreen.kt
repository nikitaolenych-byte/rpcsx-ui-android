package net.rpcsx.ui.settings

import android.content.Intent
import android.net.Uri
import android.provider.DocumentsContract
import android.util.Log
import android.view.KeyEvent
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.focusable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.KeyboardArrowLeft
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Share
import androidx.compose.material.icons.filled.Person
import androidx.compose.material3.Button
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LargeTopAppBar
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.onKeyEvent
import androidx.compose.ui.input.key.type
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.documentfile.provider.DocumentFile
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import android.content.Context
import net.rpcsx.R
import net.rpcsx.RPCSX
import net.rpcsx.UserRepository
import net.rpcsx.dialogs.AlertDialogQueue
import net.rpcsx.provider.AppDataDocumentProvider
import net.rpcsx.ui.common.ComposePreview
import net.rpcsx.ui.settings.components.core.PreferenceHeader
import net.rpcsx.ui.settings.components.core.PreferenceIcon
import net.rpcsx.ui.settings.components.core.PreferenceSubtitle
import net.rpcsx.ui.settings.components.core.PreferenceValue
import net.rpcsx.ui.settings.components.preference.HomePreference
import net.rpcsx.ui.settings.components.preference.RegularPreference
import net.rpcsx.ui.settings.components.preference.SingleSelectionDialog
import net.rpcsx.ui.settings.components.preference.SliderPreference
import net.rpcsx.ui.settings.components.preference.SwitchPreference
import net.rpcsx.utils.FileUtil
import net.rpcsx.utils.GeneralSettings
import net.rpcsx.utils.InputBindingPrefs
import net.rpcsx.utils.RpcsxUpdater
import net.rpcsx.utils.safeSettingsSet
import net.rpcsx.utils.displayToCpuToken
import net.rpcsx.utils.mapCpuPartToName
import org.json.JSONObject
import java.io.File
import kotlin.math.ceil

// (use shared helpers from net.rpcsx.utils for native safety and CPU mapping)

// Native setNCEMode not available - settings only
// private fun safeSetNCEMode(mode: Int) {
//     try {
//         RPCSX.instance.setNCEMode(mode)
//     } catch (e: Throwable) {
//         Log.e("Settings", "Error setting NCE mode to $mode: ${e.message}")
//     }
// }

private fun applyPpuLLVMTurbo(): Boolean {
    // Check if library is loaded first
    if (RPCSX.activeLibrary.value == null) {
        Log.w("Settings", "Cannot apply PPU Turbo: RPCSX library not loaded")
        return false
    }
    
    // Latest RPCS3 optimizations (Jan 2026)
    val updates = listOf(
        // Core LLVM optimizations
        "Core@@PPU LLVM Greedy Mode" to "true",
        "Core@@LLVM Precompilation" to "true",
        "Core@@Set DAZ and FTZ" to "true",
        "Core@@Thread Scheduler Mode" to "\"alt\"",
        
        // PPU performance (disable accuracy for speed)
        "Core@@PPU LLVM Java Mode Handling" to "true",
        "Core@@PPU Accurate Non-Java Mode" to "false",
        "Core@@PPU Set Saturation Bit" to "false",
        "Core@@PPU Set FPCC Bits" to "false",
        "Core@@PPU Fixup Vector NaN Values" to "false",
        "Core@@PPU Accurate Vector NaN Values" to "false",
        "Core@@Accurate Cache Line Stores" to "false",
        "Core@@Accurate PPU 128-byte Reservation Op Max Length" to "0",
        "Core@@Use Accurate DFMA" to "false",
        
        // NEW: LLVM 21 optimizations (if supported)
        "Core@@PPU LLVM Accurate VNAN" to "false",
        "Core@@PPU LLVM Accurate FCMP" to "false",
        "Core@@PPU Use Function Hashing" to "true",
        "Core@@PPU Call History" to "false",
        
        // Threading optimizations  
        "Core@@Max LLVM Compile Threads" to "8",
        "Core@@PPU Threads" to "2",
        "Core@@Lower SPU thread priority" to "false",
        
        // Memory optimizations
        "Core@@SPU Shared Memory" to "true",
        "Core@@Relaxed ZCULL Sync" to "true"
    )

    // Apply settings - continue even if some fail (key may not exist in this version)
    var successCount = 0
    for ((path, value) in updates) {
        if (safeSettingsSet(path, value)) {
            successCount++
        }
    }
    Log.i("Settings", "PPU LLVM Turbo: applied $successCount/${updates.size} settings")
    return successCount > 0  // Success if at least one setting was applied
}

private fun applySpuLLVMTurbo(): Boolean {
    // Check if library is loaded first
    if (RPCSX.activeLibrary.value == null) {
        Log.w("Settings", "Cannot apply SPU Turbo: RPCSX library not loaded")
        return false
    }
    
    // Latest RPCS3 SPU optimizations (Jan 2026)
    val updates = listOf(
        // SPU Cache & Verification
        "Core@@SPU Cache" to "true",
        "Core@@SPU Verification" to "false",
        "Core@@Precise SPU Verification" to "false",
        
        // SPU Accuracy (disable for performance)
        "Core@@SPU Accurate DMA" to "false",
        "Core@@SPU Accurate Reservations" to "false",
        "Core@@SPU Accurate GETLLAR" to "false",
        "Core@@SPU Accurate PUTLLUC" to "false",
        
        // SPU Block Size - Mega/Giga for better performance
        "Core@@SPU Block Size" to "\"giga\"",
        
        // SPU Threading
        "Core@@Preferred SPU Threads" to "6",
        "Core@@Max SPURS Threads" to "6",
        "Core@@SPU Threads" to "6",
        
        // SPU Optimizations
        "Core@@SPU loop detection" to "true",
        "Core@@SPU delay penalty" to "0",
        "Core@@XFloat Accuracy" to "\"approximate\"",
        
        // NEW: LLVM 21 SPU optimizations
        "Core@@SPU LLVM Greedy Mode" to "true",
        "Core@@SPU LLVM Lower Bound" to "0",
        "Core@@SPU LLVM Const Folding" to "true",
        "Core@@SPU LLVM Dead Code Elim" to "true",
        
        // Memory/Cache
        "Core@@SPU Shared Memory" to "true",
        "Core@@MFC Commands Shuffling" to "true"
    )

    // Apply settings - continue even if some fail (key may not exist in this version)
    var successCount = 0
    for ((path, value) in updates) {
        if (safeSettingsSet(path, value)) {
            successCount++
        }
    }
    Log.i("Settings", "SPU LLVM Turbo: applied $successCount/${updates.size} settings")
    return successCount > 0  // Success if at least one setting was applied
}

// Apply Call of Duty 4 runtime performance patch (settings-only)
private fun applyCod4Patch(): Boolean {
    if (RPCSX.activeLibrary.value == null) {
        Log.w("Settings", "Cannot apply COD4 patch: RPCSX library not loaded")
        return false
    }

    val updates = listOf(
        // Prefer newer LLVM backend for PPU and SPU
        "Core@@PPU LLVM Version" to "\"20.3\"",
        "Core@@SPU LLVM Version" to "\"20.3\"",
        "Core@@PPU LLVM Greedy Mode" to "true",

        // Reduce accuracy for speed in COD4
        "Core@@PPU Accurate Non-Java Mode" to "false",
        "Core@@PPU Set Saturation Bit" to "false",
        "Core@@Accurate Cache Line Stores" to "false",
        "Core@@Use Accurate DFMA" to "false",

        // SPU runtime performance
        "Core@@SPU Verification" to "false",
        "Core@@Precise SPU Verification" to "false",
        "Core@@SPU Accurate DMA" to "false",
        "Core@@SPU Accurate Reservations" to "false",
        "Core@@SPU Accurate GETLLAR" to "false",
        "Core@@SPU Accurate PUTLLUC" to "false",
        "Core@@SPU Block Size" to "\"giga\"",
        "Core@@SPU Threads" to "6",

        // General runtime hints
        "Core@@Max LLVM Compile Threads" to "8",
        "Core@@Lower SPU thread priority" to "false",
        "Core@@Relaxed ZCULL Sync" to "true"
    )

    var successCount = 0
    for ((path, value) in updates) {
        if (safeSettingsSet(path, value)) successCount++
    }
    Log.i("Settings", "COD4 Patch: applied $successCount/${updates.size} settings")
    return successCount > 0
}

// Safety helper: restore PPU/SPU decoder settings to known-working values
private fun restoreDecodersToSafeDefaults(): Boolean {
    if (RPCSX.activeLibrary.value == null) return false
    var ok = true
    // Force decoders to legacy LLVM tokens which are widely supported in builds
    ok = ok && safeSettingsSet("Core@@PPU Decoder", "\"LLVM Recompiler (Legacy)\"")
    ok = ok && safeSettingsSet("Core@@SPU Decoder", "\"LLVM Recompiler (Legacy)\"")
    // Hint the older LLVM version to avoid version-mismatch in native code
    ok = ok && safeSettingsSet("Core@@PPU LLVM Version", "\"19\"")
    ok = ok && safeSettingsSet("Core@@SPU LLVM Version", "\"19\"")
    return ok
}

// Detect CPU core variants for the device and present friendly names
private fun detectCpuCoreVariants(context: Context): List<String> {
    // First try to read CPU description from native systemInfo() if native lib is loaded.
    try {
        if (RPCSX.activeLibrary.value != null) {
            try {
                val sys = net.rpcsx.utils.safeNativeCall { RPCSX.instance.systemInfo() }
                if (!sys.isNullOrEmpty()) {
                    val text = sys

                    // 1) Look for explicit Cortex names in free-form text
                    val cortexRegex = Regex("(?i)cortex[- ]?[a-z0-9]+")
                    val matches = cortexRegex.findAll(text).map { m ->
                        val raw = m.value.replace("cortex ", "Cortex-", ignoreCase = true)
                            .replace("cortex-", "Cortex-", ignoreCase = true)
                        raw.replaceFirstChar { it.uppercaseChar() }
                    }.toList()

                    if (matches.isNotEmpty()) {
                        val counts = matches.groupingBy { it }.eachCount()
                        val variants = ArrayList<String>()
                        for ((name, count) in counts) {
                            variants.add(if (count > 1) "$name x$count" else name)
                        }
                        return variants
                    }
                    // fallback to further parsing of systemInfo below if needed
                }
            } catch (e: Throwable) {
                // ignore native systemInfo errors and fallback to /proc parsing
            }
        }

        val cpuinfoFile = File("/proc/cpuinfo")
        if (cpuinfoFile.exists()) {
                val text = cpuinfoFile.readText()

                // 1) Look for explicit Cortex names in free-form text
                val cortexRegex = Regex("(?i)cortex[- ]?[a-z0-9]+")
                val matches = cortexRegex.findAll(text).map { m ->
                    val raw = m.value.replace("cortex ", "Cortex-", ignoreCase = true)
                        .replace("cortex-", "Cortex-", ignoreCase = true)
                    raw.replaceFirstChar { it.uppercaseChar() }
                }.toList()

                if (matches.isNotEmpty()) {
                    val counts = matches.groupingBy { it }.eachCount()
                    val variants = ArrayList<String>()
                    for ((name, count) in counts) {
                        variants.add(if (count > 1) "$name x$count" else name)
                    }
                    return variants
                }

                // 2) Parse per-processor blocks and try to map 'CPU part' or model name -> Cortex
                try {
                    val blocks = mutableMapOf<Int, MutableList<String>>()
                    var cur = -1
                    for (line in text.lines()) {
                        val procMatch = Regex("^processor\\s*:\\s*(\\d+)", RegexOption.IGNORE_CASE).find(line)
                        if (procMatch != null) {
                            cur = procMatch.groupValues[1].toInt()
                            blocks[cur] = ArrayList()
                            continue
                        }
                        if (cur >= 0) blocks[cur]?.add(line)
                    }

                    val names = ArrayList<String>()
                    for ((_, lines) in blocks) {
                        var found: String? = null
                        // cpu part
                        for (l in lines) {
                            val cpuPartMatch = Regex("(?i)cpu part\\s*:\\s*(0x[0-9a-fA-F]+|[0-9a-fA-F]+)").find(l)
                            if (cpuPartMatch != null) {
                                val part = cpuPartMatch.groupValues[1]
                                val mapped = mapCpuPartToName(part)
                                if (mapped != null) {
                                    found = mapped
                                    break
                                }
                            }
                        }
                        if (found == null) {
                            // check model name / processor line for cortex keyword
                            for (l in lines) {
                                val cortex = cortexRegex.find(l)
                                if (cortex != null) {
                                    val raw = cortex.value.replace("cortex ", "Cortex-", ignoreCase = true).replace("cortex-", "Cortex-", ignoreCase = true)
                                    found = raw.replaceFirstChar { it.uppercaseChar() }
                                    break
                                }
                            }
                        }
                        if (found == null) {
                            // last-ditch: look for 'model name' or 'Processor' entries
                            for (l in lines) {
                                val m = Regex("(?i)(model name|processor)\\s*:\\s*(.+)").find(l)
                                if (m != null) {
                                    val v = m.groupValues[2].trim()
                                    if (v.isNotEmpty()) {
                                        found = v.split(Regex("\\s+"))[0]
                                        break
                                    }
                                }
                            }
                        }
                        if (found != null) names.add(found)
                    }

                    if (names.isNotEmpty()) {
                        val counts = names.groupingBy { it }.eachCount()
                        val variants = ArrayList<String>()
                        for ((name, count) in counts) {
                            variants.add(if (count > 1) "$name x$count" else name)
                        }
                        return variants
                    }
                } catch (e: Throwable) {
                    // ignore and continue to frequency fallback
                }
        }
    } catch (e: Throwable) {
        // ignore and fallback
    }

    // Fallback: group CPUs by max frequency (clusters)
    try {
        val cpuCount = Runtime.getRuntime().availableProcessors()
        val freqMap = mutableMapOf<Int, Int>()
        for (i in 0 until cpuCount) {
            val freqFile = File("/sys/devices/system/cpu/cpu$i/cpufreq/cpuinfo_max_freq")
            if (freqFile.exists()) {
                val freq = freqFile.readText().trim().toIntOrNull() ?: continue
                freqMap[freq] = freqMap.getOrDefault(freq, 0) + 1
            }
        }
        if (freqMap.isNotEmpty()) {
            val variants = ArrayList<String>()
            val sorted = freqMap.entries.sortedByDescending { it.key }
            var idx = 0
            for ((freq, count) in sorted) {
                val mhz = freq / 1000
                // Fallback names by cluster index if real core names are not available
                variants.add(if (count > 1) "CoreCluster@$mhz MHz x$count" else "Core@$mhz MHz")
                idx++
            }
            return variants
        }
    } catch (e: Throwable) {
        // ignore
    }

    // Final fallback: provide explicit CPU# names
    val cpuCount = Runtime.getRuntime().availableProcessors()
    val fallback = ArrayList<String>()
    for (i in 0 until cpuCount) {
        fallback.add("CPU$i")
    }
    return fallback
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AdvancedSettingsScreen(
    modifier: Modifier = Modifier,
    navigateBack: () -> Unit,
    navigateTo: (path: String) -> Unit,
    settings: JSONObject,
    path: String = "",
    mode: String = "default",  // "game", "global", "default"
    gameName: String = ""
) {
    val context = LocalContext.current
    val settingValue = remember { mutableStateOf(settings) }
    var searchQuery by remember { mutableStateOf("") }
    var isSearching by remember { mutableStateOf(false) }
    val scope = rememberCoroutineScope()


    val installRpcsxLauncher =
        rememberLauncherForActivityResult(contract = ActivityResultContracts.GetContent()) { uri: Uri? ->
            if (uri != null) {
                val target = File(context.filesDir.canonicalPath, "librpcsx-dev.so")
                if (target.exists()) {
                    target.delete()
                }

                scope.launch {
                    withContext(Dispatchers.IO) {
                        FileUtil.saveFile(context, uri, target.path)
                    }

                    try {
                        val libVer = net.rpcsx.utils.safeNativeCall { RPCSX.instance.getLibraryVersion(target.path) }
                        if (libVer != null) {
                            try {
                                RpcsxUpdater.installUpdate(context, target)
                            } catch (e: Throwable) {
                                Log.e("Settings", "Failed to install RPCSX lib: ${e.message}")
                                AlertDialogQueue.showDialog(
                                    context.getString(R.string.error),
                                    "Failed to install RPCSX library: ${e.message}"
                                )
                            }
                        } else {
                            Log.w("Settings", "Provided library at ${target.path} did not report a version")
                        }
                    } catch (e: Throwable) {
                        Log.e("Settings", "Error while validating RPCSX library: ${e.message}")
                    }
                }
            }
        }

    // UI state for LLVM CPU core picker
    var showCpuCoreDialog by remember { mutableStateOf(false) }

    // Keep variants reactive: recompute when native library status changes so
    // `systemInfo()` results are picked up after native loads.
    var llvmCpuCoreVariants by remember { mutableStateOf(detectCpuCoreVariants(context).ifEmpty { listOf("CPU0") }) }

    var llvmCpuCoreValue by remember {
        mutableStateOf(
            (GeneralSettings["llvm_cpu_core"] as? String)?.takeIf { llvmCpuCoreVariants.contains(it) }
                ?: llvmCpuCoreVariants.firstOrNull() ?: ""
        )
    }

    // Recompute variants when native library becomes available or changes
    LaunchedEffect(RPCSX.activeLibrary.value) {
        try {
            val newVariants = detectCpuCoreVariants(context).ifEmpty { listOf("CPU0") }
            if (newVariants != llvmCpuCoreVariants) {
                llvmCpuCoreVariants = newVariants
                val saved = GeneralSettings["llvm_cpu_core"] as? String
                llvmCpuCoreValue = saved?.takeIf { newVariants.contains(it) } ?: newVariants.firstOrNull() ?: ""
            }
        } catch (e: Throwable) {
            android.util.Log.w("Settings", "Failed to refresh LLVM CPU variants: ${e.message}")
        }
    }

    // When native library loads, apply saved preferences to native (best-effort)
    LaunchedEffect(RPCSX.activeLibrary.value) {
        try {
            // Apply saved LLVM CPU preference to native if present
            val savedLlvm = GeneralSettings["llvm_cpu_core"] as? String
            if (!savedLlvm.isNullOrBlank() && RPCSX.activeLibrary.value != null) {
                val token = displayToCpuToken(savedLlvm)
                val candidates = listOf(
                    "\"$token\"",
                    "'${token}'",
                    "\"${savedLlvm.trim()}\"",
                    "'${savedLlvm.trim()}'",
                    token,
                    savedLlvm.trim()
                )
                var applied = false
                for (c in candidates) {
                    try {
                        if (safeSettingsSet("Core@@LLVM CPU Core", c)) {
                            applied = true
                            android.util.Log.i("Settings", "Applied saved LLVM CPU to native: $c")
                            break
                        }
                    } catch (_: Throwable) { }
                }
                if (!applied) android.util.Log.w("Settings", "Could not apply saved LLVM CPU to native: $savedLlvm")
            }

            // Apply saved SVE2 flag automatically (no visible toggle)
            val savedSve2 = GeneralSettings["enable_sve2"] as? Boolean ?: false
            try {
                if (RPCSX.activeLibrary.value != null) {
                    safeSettingsSet("Core@@NCE Enable SVE2", if (savedSve2) "true" else "false")
                    android.util.Log.i("Settings", "Applied saved SVE2=${savedSve2} to native")
                }
            } catch (e: Throwable) {
                android.util.Log.w("Settings", "Failed to apply SVE2 to native: ${e.message}")
            }
        } catch (e: Throwable) {
            android.util.Log.w("Settings", "Error applying saved native prefs: ${e.message}")
        }
    }

    val topBarScrollBehavior = TopAppBarDefaults.enterAlwaysScrollBehavior()
    Scaffold(
        modifier = Modifier
            .nestedScroll(topBarScrollBehavior.nestedScrollConnection)
            .then(modifier), topBar = {
            // Use nice title based on mode instead of raw path
            val screenTitle = when (mode) {
                "game" -> if (gameName.isNotBlank()) gameName else stringResource(R.string.custom_configuration)
                "global" -> stringResource(R.string.global_settings)
                "default" -> stringResource(R.string.advanced_settings)
                else -> if (path.isNotBlank()) {
                    // Fallback for navigation from SettingsScreen
                    path.replace("@@", " / ").removePrefix(" / ").substringAfterLast(" / ")
                } else stringResource(R.string.advanced_settings)
            }
            LargeTopAppBar(title = {
                if (isSearching) {
                    BasicTextField(
                        value = searchQuery,
                        onValueChange = { searchQuery = it },
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 8.dp),
                        singleLine = true,
                        textStyle = TextStyle(
                            color = MaterialTheme.colorScheme.onSurface, fontSize = 20.sp
                        ),
                        decorationBox = { innerTextField ->
                            Box(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .background(
                                        MaterialTheme.colorScheme.surfaceVariant,
                                        shape = RoundedCornerShape(8.dp)
                                    )
                                    .padding(8.dp)
                            ) {
                                if (searchQuery.isEmpty()) {
                                    Text(
                                        text = stringResource(R.string.search_settings),
                                        color = MaterialTheme.colorScheme.onSurfaceVariant
                                    )
                                }
                                innerTextField()
                            }
                        })
                } else {
                    Text(
                        text = screenTitle,
                        fontWeight = FontWeight.Medium,
                        maxLines = 1,
                        overflow = androidx.compose.ui.text.style.TextOverflow.Ellipsis
                    )
                }
            }, scrollBehavior = topBarScrollBehavior, navigationIcon = {
                IconButton(onClick = navigateBack) {
                    Icon(
                        imageVector = Icons.Filled.KeyboardArrowLeft,
                        contentDescription = null
                    )
                }
            }, actions = {
                // (decoder fixer removed)
                IconButton(
                    onClick = {
                        if (isSearching) {
                            searchQuery = ""
                            isSearching = false
                        } else {
                            isSearching = true
                        }
                    }) {
                    Icon(
                        imageVector = if (isSearching) Icons.Default.Close else Icons.Default.Search,
                        contentDescription = if (isSearching) "Close Search" else "Search"
                    )
                }
            })
        }) { contentPadding ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(contentPadding),
        ) {
            // Core Settings - show only in Core section
            // Note: ARM64 NEON optimizations are now baked into LLVM code (v1.5.0-neon)
            // See: SPULLVMRecompiler.cpp, CPUTranslator.h for real NEON implementations
            if (path.contains("Core") || path.endsWith("@@Core")) {
                item(key = "core_settings_header") {
                    PreferenceHeader(text = "Core Settings")
                }
                // LLVM CPU selector inside Core section (affinity hint for LLVM JIT)
                item(key = "llvm_cpu_core_setting") {
                    // Show preference row
                    RegularPreference(
                        title = "LLVM CPU",
                        leadingIcon = null,
                        onClick = { showCpuCoreDialog = true }
                    )

                    // Selection dialog
                    if (showCpuCoreDialog) {
                        ModalBottomSheet(onDismissRequest = { showCpuCoreDialog = false }) {
                            Column(modifier = Modifier.padding(12.dp)) {
                                SingleSelectionDialog(
                                    currentValue = llvmCpuCoreValue,
                                    values = llvmCpuCoreVariants,
                                    icon = null,
                                    title = "Select LLVM CPU Core",
                                    onValueChange = { value ->
                                        // Store UI preference immediately so selection always 'sticks'
                                        GeneralSettings.setValue("llvm_cpu_core", value)
                                        llvmCpuCoreValue = value

                                        // Try several write formats for native settings to maximise compatibility.
                                        val token = displayToCpuToken(value)
                                        var applied = false
                                        val candidates = listOf(
                                            "\"$token\"",
                                            "'${token}'",
                                            "\"${value.trim()}\"",
                                            "'${value.trim()}'",
                                            token,
                                            value.trim()
                                        )

                                        for (c in candidates) {
                                            try {
                                                if (safeSettingsSet("Core@@LLVM CPU Core", c)) {
                                                    applied = true
                                                    android.util.Log.i("Settings", "Applied LLVM CPU native value for Core@@LLVM CPU Core: $c (from display '$value')")

                                                    // Read back native settings and system info for debugging
                                                    try {
                                                        val settingsJson = net.rpcsx.utils.safeNativeCall { RPCSX.instance.settingsGet("") }
                                                        if (settingsJson != null) {
                                                            android.util.Log.i("Settings", "Native settings snapshot after write: $settingsJson")
                                                        } else {
                                                            android.util.Log.w("Settings", "settingsGet returned null after writing LLVM CPU core")
                                                        }
                                                    } catch (e: Throwable) {
                                                        android.util.Log.e("Settings", "Error reading native settings after write: ${e.message}", e)
                                                    }

                                                    try {
                                                        val sys = net.rpcsx.utils.safeNativeCall { RPCSX.instance.systemInfo() }
                                                        if (sys != null) android.util.Log.i("Settings", "Native systemInfo after write: $sys")
                                                    } catch (e: Throwable) {
                                                        android.util.Log.e("Settings", "Error calling systemInfo after write: ${e.message}", e)
                                                    }

                                                    break
                                                }
                                            } catch (e: Throwable) {
                                                // continue trying other formats
                                            }
                                        }

                                        if (!applied) {
                                            // Log native failure only; do not show UI error. Preference saved locally.
                                            android.util.Log.w("Settings", "Failed to apply LLVM CPU core to native (tried ${candidates.size} formats): $value")
                                        }

                                        showCpuCoreDialog = false
                                    },
                                    onLongClick = {}
                                )
                            }
                        }
                    }
                    // (custom input removed — selection uses only detected core names)
                }

                // SVE2 is applied automatically based on device capability and saved preference.
                // No manual toggle is shown here to avoid user confusion or unsafe toggles.
            }
            
            // Dynamic Resolution Scaling (DRS) section - show in GPU or Video section
            if (path.contains("Video") || path.contains("GPU") || path.contains("Graphics") || path.isEmpty()) {
                item(key = "drs_header") {
                    PreferenceHeader(text = "Dynamic Resolution Scaling (DRS)")
                }
                
                // DRS Mode selector
                item(key = "drs_mode_setting") {
                    var drsMode by remember { 
                        mutableStateOf(GeneralSettings["drs_mode"] as? Int ?: 0) 
                    }
                    val drsModes = listOf("Disabled", "Performance", "Balanced", "Quality")
                    
                    SingleSelectionDialog(
                        currentValue = drsModes.getOrElse(drsMode) { "Disabled" },
                        values = drsModes,
                        icon = null,
                        title = "DRS Mode",
                        onValueChange = { value ->
                            val newMode = drsModes.indexOf(value)
                            drsMode = newMode
                            GeneralSettings.setValue("drs_mode", newMode)
                            try {
                                RPCSX.instance.drsSetMode(newMode)
                            } catch (e: Throwable) {
                                android.util.Log.w("DRS", "Failed to set DRS mode: ${e.message}")
                            }
                        },
                        onLongClick = {}
                    )
                }
                
                // Target FPS selector
                item(key = "drs_target_fps_setting") {
                    var targetFps by remember { 
                        mutableStateOf(GeneralSettings["drs_target_fps"] as? Int ?: 30) 
                    }
                    val fpsOptions = listOf("30", "60", "120")
                    
                    SingleSelectionDialog(
                        currentValue = targetFps.toString(),
                        values = fpsOptions,
                        icon = null,
                        title = "DRS Target FPS",
                        onValueChange = { value ->
                            val fps = value.toIntOrNull() ?: 30
                            targetFps = fps
                            GeneralSettings.setValue("drs_target_fps", fps)
                            try {
                                RPCSX.instance.drsSetTargetFPS(fps)
                            } catch (e: Throwable) {
                                android.util.Log.w("DRS", "Failed to set target FPS: ${e.message}")
                            }
                        },
                        onLongClick = {}
                    )
                }
                
                // Min Scale slider
                item(key = "drs_min_scale_setting") {
                    var minScale by remember { 
                        mutableStateOf((GeneralSettings["drs_min_scale"] as? Float ?: 0.5f) * 100f) 
                    }
                    
                    SliderPreference(
                        title = "DRS Minimum Scale",
                        value = minScale,
                        valueRange = 25f..100f,
                        steps = 14,
                        leadingIcon = null,
                        subtitle = "${minScale.toInt()}%",
                        onValueChange = { value ->
                            minScale = value
                            val scale = value / 100f
                            GeneralSettings.setValue("drs_min_scale", scale)
                            try {
                                RPCSX.instance.drsSetMinScale(scale)
                            } catch (e: Throwable) {
                                android.util.Log.w("DRS", "Failed to set min scale: ${e.message}")
                            }
                        }
                    )
                }
                
                // FSR Upscaling toggle
                item(key = "drs_fsr_upscaling_setting") {
                    var fsrEnabled by remember { 
                        mutableStateOf(GeneralSettings["drs_fsr_upscaling"] as? Boolean ?: true) 
                    }
                    
                    SwitchPreference(
                        checked = fsrEnabled,
                        title = "FSR Upscaling",
                        leadingIcon = null,
                        onClick = { value ->
                            fsrEnabled = value
                            GeneralSettings.setValue("drs_fsr_upscaling", value)
                            try {
                                RPCSX.instance.drsSetFSRUpscaling(value)
                            } catch (e: Throwable) {
                                android.util.Log.w("DRS", "Failed to set FSR upscaling: ${e.message}")
                            }
                        },
                        onLongClick = {}
                    )
                }
            }
            
            // Texture Streaming Cache section
            if (path.contains("Video") || path.contains("GPU") || path.contains("Graphics") || path.isEmpty()) {
                item(key = "texture_streaming_header") {
                    PreferenceHeader(text = "Texture Streaming Cache")
                }
                
                item(key = "texture_streaming_enabled") {
                    var enabled by remember { 
                        mutableStateOf(GeneralSettings["texture_streaming_enabled"] as? Boolean ?: true) 
                    }
                    
                    SwitchPreference(
                        checked = enabled,
                        title = "Enable Texture Streaming",
                        leadingIcon = null,
                        onClick = { value ->
                            enabled = value
                            GeneralSettings.setValue("texture_streaming_enabled", value)
                        },
                        onLongClick = {}
                    )
                }
                
                item(key = "texture_streaming_mode") {
                    var mode by remember { 
                        mutableStateOf(GeneralSettings["texture_streaming_mode"] as? Int ?: 2) 
                    }
                    val modes = listOf("Off", "Minimal", "Balanced", "Aggressive", "Ultra")
                    
                    SingleSelectionDialog(
                        currentValue = modes.getOrElse(mode) { "Balanced" },
                        values = modes,
                        icon = null,
                        title = "Streaming Mode",
                        onValueChange = { value ->
                            val newMode = modes.indexOf(value)
                            mode = newMode
                            GeneralSettings.setValue("texture_streaming_mode", newMode)
                            try {
                                RPCSX.instance.textureStreamingSetMode(newMode)
                            } catch (e: Throwable) {
                                android.util.Log.w("TextureStreaming", "Failed to set mode: ${e.message}")
                            }
                        },
                        onLongClick = {}
                    )
                }
                
                item(key = "texture_cache_size") {
                    var cacheSize by remember { 
                        mutableStateOf((GeneralSettings["texture_cache_size_mb"] as? Int ?: 256).toFloat()) 
                    }
                    
                    SliderPreference(
                        title = "Cache Size",
                        value = cacheSize,
                        valueRange = 64f..1024f,
                        steps = 14,
                        leadingIcon = null,
                        subtitle = "${cacheSize.toInt()} MB",
                        onValueChange = { value ->
                            cacheSize = value
                            GeneralSettings.setValue("texture_cache_size_mb", value.toInt())
                        }
                    )
                }
            }
            
            // Pipeline Cache section
            if (path.contains("Video") || path.contains("GPU") || path.contains("Graphics") || path.isEmpty()) {
                item(key = "pipeline_cache_header") {
                    PreferenceHeader(text = "Vulkan Pipeline Cache")
                }
                
                item(key = "pipeline_cache_enabled") {
                    var enabled by remember { 
                        mutableStateOf(GeneralSettings["pipeline_cache_enabled"] as? Boolean ?: true) 
                    }
                    
                    SwitchPreference(
                        checked = enabled,
                        title = "Enable Pipeline Cache",
                        leadingIcon = null,
                        onClick = { value ->
                            enabled = value
                            GeneralSettings.setValue("pipeline_cache_enabled", value)
                        },
                        onLongClick = {}
                    )
                }
                
                item(key = "pipeline_precompilation") {
                    var enabled by remember { 
                        mutableStateOf(GeneralSettings["pipeline_precompilation"] as? Boolean ?: true) 
                    }
                    
                    SwitchPreference(
                        checked = enabled,
                        title = "Async Precompilation",
                        leadingIcon = null,
                        onClick = { value ->
                            enabled = value
                            GeneralSettings.setValue("pipeline_precompilation", value)
                        },
                        onLongClick = {}
                    )
                }
                
                item(key = "pipeline_max_cached") {
                    var maxPipelines by remember { 
                        mutableStateOf((GeneralSettings["pipeline_max_cached"] as? Int ?: 2000).toFloat()) 
                    }
                    
                    SliderPreference(
                        title = "Max Cached Pipelines",
                        value = maxPipelines,
                        valueRange = 500f..5000f,
                        steps = 8,
                        leadingIcon = null,
                        subtitle = "${maxPipelines.toInt()}",
                        onValueChange = { value ->
                            maxPipelines = value
                            GeneralSettings.setValue("pipeline_max_cached", value.toInt())
                        }
                    )
                }
            }
            
            // Game Profiles section
            if (path.isEmpty() || path.contains("Game") || path.contains("CPU") || path.contains("Emulation")) {
                item(key = "game_profiles_header") {
                    PreferenceHeader(text = "Game Performance Profiles")
                }
                
                item(key = "auto_apply_profiles") {
                    var enabled by remember { 
                        mutableStateOf(GeneralSettings["auto_apply_profiles"] as? Boolean ?: true) 
                    }
                    
                    SwitchPreference(
                        checked = enabled,
                        title = "Auto-Apply Game Profiles",
                        leadingIcon = null,
                        onClick = { value ->
                            enabled = value
                            GeneralSettings.setValue("auto_apply_profiles", value)
                        },
                        onLongClick = {}
                    )
                }
                
                item(key = "use_community_profiles") {
                    var enabled by remember { 
                        mutableStateOf(GeneralSettings["use_community_profiles"] as? Boolean ?: false) 
                    }
                    
                    SwitchPreference(
                        checked = enabled,
                        title = "Use Community Profiles",
                        leadingIcon = null,
                        onClick = { value ->
                            enabled = value
                            GeneralSettings.setValue("use_community_profiles", value)
                        },
                        onLongClick = {}
                    )
                }
                
                item(key = "profiles_stats") {
                    var statsJson by remember { mutableStateOf("{}") }
                    
                    LaunchedEffect(Unit) {
                        try {
                            statsJson = RPCSX.instance.profilesGetStatsJson()
                        } catch (e: Throwable) {
                            android.util.Log.w("Profiles", "Failed to get stats: ${e.message}")
                        }
                    }
                    
                    RegularPreference(
                        title = "Profiles Statistics",
                        leadingIcon = null,
                        trailingContent = {
                            Text(
                                text = try {
                                    val json = org.json.JSONObject(statsJson)
                                    "${json.optInt("total_profiles", 0)} profiles"
                                } catch (e: Throwable) { "N/A" },
                                style = MaterialTheme.typography.bodySmall
                            )
                        },
                        onClick = {}
                    )
                }
            }
            
            // SVE2/NEON Optimizations section
            if (path.isEmpty() || path.contains("CPU") || path.contains("Advanced")) {
                item(key = "sve2_header") {
                    PreferenceHeader(text = "ARM SIMD Optimizations")
                }
                
                item(key = "sve2_status") {
                    var capsJson by remember { mutableStateOf("{}") }
                    
                    LaunchedEffect(Unit) {
                        try {
                            capsJson = RPCSX.instance.sve2GetCapabilitiesJson()
                        } catch (e: Throwable) {
                            android.util.Log.w("SVE2", "Failed to get caps: ${e.message}")
                        }
                    }
                    
                    val statusText = try {
                        val json = org.json.JSONObject(capsJson)
                        when {
                            json.optBoolean("has_sve2", false) -> "SVE2 (${json.optInt("sve_vector_length", 0)} bit)"
                            json.optBoolean("has_sve", false) -> "SVE (${json.optInt("sve_vector_length", 0)} bit)"
                            json.optBoolean("has_neon", false) -> "NEON"
                            else -> "None"
                        }
                    } catch (e: Throwable) { "Unknown" }
                    
                    RegularPreference(
                        title = "SIMD Capabilities",
                        leadingIcon = null,
                        trailingContent = {
                            Text(
                                text = statusText,
                                style = MaterialTheme.typography.bodySmall,
                                color = if (statusText.contains("SVE2")) MaterialTheme.colorScheme.primary 
                                       else MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        },
                        onClick = {}
                    )
                }
                
                item(key = "enable_sve2_optimizations") {
                    var enabled by remember { 
                        mutableStateOf(GeneralSettings["enable_sve2_optimizations"] as? Boolean ?: true) 
                    }
                    
                    SwitchPreference(
                        checked = enabled,
                        title = "Use SVE2/NEON Optimizations",
                        leadingIcon = null,
                        onClick = { value ->
                            enabled = value
                            GeneralSettings.setValue("enable_sve2_optimizations", value)
                        },
                        onLongClick = {}
                    )
                }
            }

            // PS3 Compatibility Settings section
            if (path.isEmpty() || path.contains("PS3") || path.contains("Compatibility") || path.contains("Emulation")) {
                item(key = "ps3_compat_header") {
                    PreferenceHeader(text = "PS3 Compatibility")
                }
                
                // Firmware Spoofing
                item(key = "firmware_spoof_version") {
                    var version by remember { mutableStateOf("4.90") }
                    
                    LaunchedEffect(Unit) {
                        try {
                            version = RPCSX.instance.firmwareSpoofGetVersion()
                        } catch (e: Throwable) {
                            android.util.Log.w("FirmwareSpoof", "Failed to get version: ${e.message}")
                        }
                    }
                    
                    RegularPreference(
                        title = "Firmware Version Spoof",
                        leadingIcon = null,
                        trailingContent = {
                            Text(
                                text = version,
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.primary
                            )
                        },
                        onClick = {}
                    )
                }
                
                // Syscall Stubs
                item(key = "syscall_stubs_status") {
                    var statsJson by remember { mutableStateOf("{}") }
                    
                    LaunchedEffect(Unit) {
                        try {
                            statsJson = RPCSX.instance.syscallStubsGetStats()
                        } catch (e: Throwable) {
                            android.util.Log.w("SyscallStubs", "Failed to get stats: ${e.message}")
                        }
                    }
                    
                    RegularPreference(
                        title = "Syscall Stubbing Status",
                        leadingIcon = null,
                        trailingContent = {
                            Text(
                                text = try {
                                    val json = org.json.JSONObject(statsJson)
                                    val stubbed = json.optInt("stubbed_calls", 0)
                                    val total = json.optInt("total_calls", 0)
                                    if (total > 0) "$stubbed stubbed" else "Ready"
                                } catch (e: Throwable) { "N/A" },
                                style = MaterialTheme.typography.bodySmall
                            )
                        },
                        onClick = {}
                    )
                }
                
                // Library Emulation
                item(key = "library_emulation_status") {
                    var statsJson by remember { mutableStateOf("{}") }
                    
                    LaunchedEffect(Unit) {
                        try {
                            statsJson = RPCSX.instance.libraryEmulationGetStats()
                        } catch (e: Throwable) {
                            android.util.Log.w("LibraryEmulation", "Failed to get stats: ${e.message}")
                        }
                    }
                    
                    RegularPreference(
                        title = "Library Emulation Layer",
                        leadingIcon = null,
                        trailingContent = {
                            Text(
                                text = try {
                                    val json = org.json.JSONObject(statsJson)
                                    "${json.optInt("total_libraries", 0)} libraries"
                                } catch (e: Throwable) { "N/A" },
                                style = MaterialTheme.typography.bodySmall
                            )
                        },
                        onClick = {}
                    )
                }
                
                // Patch Installer
                item(key = "patch_installer_status") {
                    var statsJson by remember { mutableStateOf("{}") }
                    
                    LaunchedEffect(Unit) {
                        try {
                            statsJson = RPCSX.instance.patchInstallerGetStats()
                        } catch (e: Throwable) {
                            android.util.Log.w("PatchInstaller", "Failed to get stats: ${e.message}")
                        }
                    }
                    
                    RegularPreference(
                        title = "Game Patch Installer",
                        leadingIcon = null,
                        trailingContent = {
                            Text(
                                text = try {
                                    val json = org.json.JSONObject(statsJson)
                                    "${json.optInt("total_cached", 0)} cached"
                                } catch (e: Throwable) { "N/A" },
                                style = MaterialTheme.typography.bodySmall
                            )
                        },
                        onClick = {}
                    )
                }
                
                item(key = "auto_apply_patches") {
                    var enabled by remember { 
                        mutableStateOf(GeneralSettings["auto_apply_patches"] as? Boolean ?: true) 
                    }
                    
                    SwitchPreference(
                        checked = enabled,
                        title = "Auto-Apply Recommended Patches",
                        leadingIcon = null,
                        onClick = { value ->
                            enabled = value
                            GeneralSettings.setValue("auto_apply_patches", value)
                        },
                        onLongClick = {}
                    )
                }
            }

            // Save Data Tools section
            if (path.isEmpty() || path.contains("Save") || path.contains("Data") || path.contains("Storage")) {
                item(key = "save_tools_header") {
                    PreferenceHeader(text = "Save Data Tools")
                }
                
                item(key = "save_converter_status") {
                    var statsJson by remember { mutableStateOf("{}") }
                    
                    LaunchedEffect(Unit) {
                        try {
                            statsJson = RPCSX.instance.saveConverterGetStats()
                        } catch (e: Throwable) {
                            android.util.Log.w("SaveConverter", "Failed to get stats: ${e.message}")
                        }
                    }
                    
                    RegularPreference(
                        title = "Save Converter Statistics",
                        leadingIcon = null,
                        trailingContent = {
                            Text(
                                text = try {
                                    val json = org.json.JSONObject(statsJson)
                                    "${json.optInt("converted", 0)} converted"
                                } catch (e: Throwable) { "N/A" },
                                style = MaterialTheme.typography.bodySmall
                            )
                        },
                        onClick = {}
                    )
                }
                
                item(key = "auto_backup_saves") {
                    var enabled by remember { 
                        mutableStateOf(GeneralSettings["auto_backup_saves"] as? Boolean ?: true) 
                    }
                    
                    SwitchPreference(
                        checked = enabled,
                        title = "Auto-Backup Saves Before Conversion",
                        leadingIcon = null,
                        onClick = { value ->
                            enabled = value
                            GeneralSettings.setValue("auto_backup_saves", value)
                        },
                        onLongClick = {}
                    )
                }
                
                item(key = "cross_region_saves") {
                    var enabled by remember { 
                        mutableStateOf(GeneralSettings["cross_region_saves"] as? Boolean ?: false) 
                    }
                    
                    SwitchPreference(
                        checked = enabled,
                        title = "Enable Cross-Region Save Compatibility",
                        leadingIcon = null,
                        onClick = { value ->
                            enabled = value
                            GeneralSettings.setValue("cross_region_saves", value)
                        },
                        onLongClick = {}
                    )
                }
            }
            
            val filteredKeys =
                settings.keys().asSequence().filter { it.contains(searchQuery, ignoreCase = true) }
                    .toList()

            filteredKeys.forEach { key ->
                val itemPath = "$path@@$key"
                item(key = key) {
                    val itemObject = settingValue.value[key] as? JSONObject

                    if (itemObject != null) {
                        when (val type =
                            if (itemObject.has("type")) itemObject.getString("type") else null) {
                            null -> {
                                RegularPreference(
                                    title = key, leadingIcon = null, onClick = {
                                        Log.e(
                                            "Main",
                                            "Navigate to settings$itemPath, object $itemObject"
                                        )
                                        navigateTo("settings$itemPath")
                                    })
                            }

                            "bool" -> {
                                var itemValue by remember { mutableStateOf(itemObject.getBoolean("value")) }
                                val def = itemObject.getBoolean("default")
                                SwitchPreference(
                                    checked = itemValue,
                                    title = key + if (itemValue == def) "" else " *",
                                    leadingIcon = null,
                                    onClick = { value ->
                                        if (!safeSettingsSet(
                                                itemPath, if (value) "true" else "false"
                                            )
                                        ) {
                                            AlertDialogQueue.showDialog(
                                                context.getString(R.string.error),
                                                context.getString(R.string.failed_to_assign_value, value.toString(), itemPath)
                                            )
                                        } else {
                                            itemObject.put("value", value)
                                            itemValue = value
                                        }
                                    },
                                    onLongClick = {
                                        AlertDialogQueue.showDialog(
                                            title = context.getString(R.string.reset_setting),
                                            message = context.getString(R.string.ask_if_reset_key, key),
                                            onConfirm = {
                                                if (safeSettingsSet(itemPath, def.toString())) {
                                                    itemObject.put("value", def)
                                                    itemValue = def
                                                } else {
                                                    AlertDialogQueue.showDialog(
                                                        context.getString(R.string.error),
                                                        context.getString(R.string.failed_to_reset_key, key)
                                                    )
                                                }
                                            })
                                    })
                            }

                            "enum" -> {
                                var itemValue by remember { mutableStateOf(itemObject.getString("value")) }
                                val def = itemObject.getString("default")
                                val variantsJson = itemObject.getJSONArray("variants")
                                val variants = ArrayList<String>()
                                for (i in 0 until variantsJson.length()) {
                                    variants.add(variantsJson.getString(i))
                                }
                                
                                // Check if this is PPU Decoder setting
                                val isPpuDecoder = key == "PPU Decoder" || 
                                                   itemPath.contains("PPU Decoder") ||
                                                   key.contains("PPU")
                                val isSpuDecoder = key == "SPU Decoder" ||
                                                   itemPath.contains("SPU Decoder") ||
                                                   key.contains("SPU")
                                
                                // Replace legacy LLVM names with LLVM 19
                                val updatedVariants = ArrayList<String>()
                                for (variant in variants) {
                                    when (variant) {
                                        "LLVM Recompiler (Legacy)" -> updatedVariants.add("LLVM 19")
                                        "Interpreter (Legacy)" -> updatedVariants.add("Interpreter")
                                        else -> updatedVariants.add(variant)
                                    }
                                }
                                variants.clear()
                                variants.addAll(updatedVariants)
                                
                                // Add NCE option to PPU Decoder if not present
                                if (isPpuDecoder && !variants.contains("NCE")) {
                                    variants.add(0, "NCE") // Add at top of list
                                }
                                
                                // Add LLVM Turbo option to PPU and SPU Decoder
                                if (isPpuDecoder && !variants.contains("LLVM Turbo")) {
                                    // Insert after NCE, before LLVM 19
                                    val nceIndex = variants.indexOf("NCE")
                                    variants.add(if (nceIndex >= 0) nceIndex + 1 else 0, "LLVM Turbo")
                                }
                                if (isSpuDecoder && !variants.contains("LLVM Turbo")) {
                                    variants.add(0, "LLVM Turbo") // Add at top
                                }

                                // Add LLVM 20.3 option (binds to new LLVM backend) to original enums
                                if (isPpuDecoder && !variants.contains("LLVM 20.3")) {
                                    // Place before LLVM 19 if present
                                    val llvm19Index = variants.indexOf("LLVM 19")
                                    variants.add(if (llvm19Index >= 0) llvm19Index else variants.size, "LLVM 20.3")
                                }
                                if (isSpuDecoder && !variants.contains("LLVM 20.3")) {
                                    // Place after Turbo for SPU
                                    val turboIndex = variants.indexOf("LLVM Turbo")
                                    variants.add(if (turboIndex >= 0) turboIndex + 1 else 0, "LLVM 20.3")
                                }

                                // Add Modified LLVM option (LLVM-HCJIT-PS3VEC with ARM SVE2/SME)
                                if (isPpuDecoder && !variants.contains("Modified LLVM")) {
                                    // Place after LLVM 20.3
                                    val llvm203Index = variants.indexOf("LLVM 20.3")
                                    variants.add(if (llvm203Index >= 0) llvm203Index + 1 else variants.size, "Modified LLVM")
                                }
                                if (isSpuDecoder && !variants.contains("Modified LLVM")) {
                                    val llvm203Index = variants.indexOf("LLVM 20.3")
                                    variants.add(if (llvm203Index >= 0) llvm203Index + 1 else variants.size, "Modified LLVM")
                                }
                                
                                // Reactive state for turbo flags - updates UI immediately
                                var ppuTurboEnabled by remember { mutableStateOf(GeneralSettings["ppu_llvm_turbo"] as? Boolean ?: false) }
                                var spuTurboEnabled by remember { mutableStateOf(GeneralSettings["spu_llvm_turbo"] as? Boolean ?: false) }

                                // Persisted requested LLVM versions (user intent) so the UI can show
                                // `LLVM 20.3` even when native stores legacy token names.
                                val savedPpuRequested = GeneralSettings["ppu_requested_llvm_version"] as? String
                                val savedSpuRequested = GeneralSettings["spu_requested_llvm_version"] as? String
                                
                                // Check if NCE mode is active (use cached value for performance)
                                val savedNceMode = net.rpcsx.utils.GeneralSettings.nceMode
                                
                                // Compute display value based on current state
                                val displayValue = when {
                                    isPpuDecoder && savedNceMode == 3 -> "NCE"
                                    isPpuDecoder && ppuTurboEnabled && itemValue == "LLVM Recompiler (Legacy)" -> "LLVM Turbo"
                                    isPpuDecoder && itemValue == "LLVM Recompiler (Legacy)" -> "LLVM 19"
                                    isPpuDecoder && (itemValue.contains("20.3") || savedPpuRequested == "20.3") -> "LLVM 20.3"
                                    isPpuDecoder && (itemValue.contains("Modified") || savedPpuRequested == "Modified") -> "Modified LLVM"
                                    isPpuDecoder && itemValue == "Interpreter (Legacy)" -> "Interpreter"
                                    isSpuDecoder && spuTurboEnabled && itemValue.contains("LLVM", ignoreCase = true) -> "LLVM Turbo"
                                    isSpuDecoder && (itemValue.contains("20.3") || savedSpuRequested == "20.3") -> "LLVM 20.3"
                                    isSpuDecoder && (itemValue.contains("Modified") || savedSpuRequested == "Modified") -> "Modified LLVM"
                                    else -> itemValue
                                }

                                Column {
                                    SingleSelectionDialog(
                                        currentValue = if (displayValue in variants) displayValue else variants[0],
                                        values = variants,
                                        icon = null,
                                        title = key + if (itemValue == def) "" else " *",
                                        onValueChange = { value ->
                                            // Special handling for Modified LLVM (LLVM-HCJIT-PS3VEC)
                                            if (isPpuDecoder && value == "Modified LLVM") {
                                                // Modified LLVM uses PS3-specific auto-vectorization with ARM SVE2/SME
                                                if (!safeSettingsSet(itemPath, "\"LLVM Recompiler (Legacy)\"")) {
                                                    AlertDialogQueue.showDialog(
                                                        context.getString(R.string.error),
                                                        context.getString(R.string.failed_to_assign_value, value, itemPath)
                                                    )
                                                }
                                                // Enable LLVM-PS3 specific settings
                                                safeSettingsSet("Core@@PPU LLVM Version", "\"PS3-HCJIT\"")
                                                safeSettingsSet("Core@@LLVM PS3 Pattern Match", "true")
                                                safeSettingsSet("Core@@LLVM PS3 Aggressive Vectorize", "true")
                                                safeSettingsSet("Core@@LLVM ARM SVE2", "true")
                                                GeneralSettings.setValue("ppu_requested_llvm_version", "Modified")
                                                GeneralSettings.setValue("llvm_ps3_enabled", true)
                                                try {
                                                    itemObject.put("value", "LLVM Recompiler (Modified)")
                                                    itemValue = "LLVM Recompiler (Modified)"
                                                } catch (e: Throwable) { }
                                                android.util.Log.i("RPCSX-LLVM", "╔════════════════════════════════════════╗")
                                                android.util.Log.i("RPCSX-LLVM", "║   Modified LLVM Activated!             ║")
                                                android.util.Log.i("RPCSX-LLVM", "║   LLVM-HCJIT-PS3VEC Enabled            ║")
                                                android.util.Log.i("RPCSX-LLVM", "║   ARM SVE2/SME Intrinsics Active       ║")
                                                android.util.Log.i("RPCSX-LLVM", "╚════════════════════════════════════════╝")
                                                return@SingleSelectionDialog
                                            }
                                            if (isSpuDecoder && value == "Modified LLVM") {
                                                val spuInternal = "Recompiler (LLVM)"
                                                if (!safeSettingsSet(itemPath, "\"$spuInternal\"")) {
                                                    safeSettingsSet(itemPath, "\"LLVM Recompiler (Legacy)\"")
                                                }
                                                safeSettingsSet("Core@@SPU LLVM Version", "\"PS3-HCJIT\"")
                                                safeSettingsSet("Core@@LLVM PS3 Pattern Match", "true")
                                                safeSettingsSet("Core@@LLVM PS3 Aggressive Vectorize", "true")
                                                GeneralSettings.setValue("spu_requested_llvm_version", "Modified")
                                                GeneralSettings.setValue("llvm_ps3_enabled", true)
                                                try {
                                                    itemObject.put("value", "Recompiler (LLVM-Modified)")
                                                    itemValue = "Recompiler (LLVM-Modified)"
                                                } catch (e: Throwable) { }
                                                return@SingleSelectionDialog
                                            }
                                            // Special handling for LLVM 20.3 - select new LLVM backend
                                            if (isPpuDecoder && value == "LLVM 20.3") {
                                                // Many native builds may not expose a modern PPU enum value.
                                                // Use the legacy LLVM recompiler token so settingsSet succeeds,
                                                // and separately write the desired LLVM version key.
                                                if (!safeSettingsSet(itemPath, "\"LLVM Recompiler (Legacy)\"")) {
                                                    AlertDialogQueue.showDialog(
                                                        context.getString(R.string.error),
                                                        context.getString(R.string.failed_to_assign_value, value, itemPath)
                                                    )
                                                }
                                                safeSettingsSet("Core@@PPU LLVM Version", "\"20.3\"")
                                                // Persist user intent so UI shows 20.3 even if native stores legacy token
                                                GeneralSettings.setValue("ppu_requested_llvm_version", "20.3")
                                                try {
                                                    // Persist value with 20.3 marker so UI shows LLVM 20.3 on reload
                                                    itemObject.put("value", "LLVM Recompiler (Legacy) 20.3")
                                                    itemValue = "LLVM Recompiler (Legacy) 20.3"
                                                } catch (e: Throwable) { }
                                                return@SingleSelectionDialog
                                            }
                                            if (isSpuDecoder && value == "LLVM 20.3") {
                                                // SPU may already support generic LLVM token; try to set it,
                                                // fall back to legacy token if needed. Keep version key as hint.
                                                val spuInternal = "Recompiler (LLVM)"
                                                if (!safeSettingsSet(itemPath, "\"$spuInternal\"")) {
                                                    // fallback to legacy token
                                                    safeSettingsSet(itemPath, "\"LLVM Recompiler (Legacy)\"")
                                                }
                                                safeSettingsSet("Core@@SPU LLVM Version", "\"20.3\"")
                                                // Persist user intent for SPU
                                                GeneralSettings.setValue("spu_requested_llvm_version", "20.3")
                                                try {
                                                    // Persist SPU value including version hint so UI shows 20.3 on reload
                                                    val preferred = "Recompiler (LLVM) 20.3"
                                                    itemObject.put("value", preferred)
                                                    itemValue = preferred
                                                } catch (e: Throwable) { }
                                                return@SingleSelectionDialog
                                            }
                                            // Special handling for LLVM Turbo - PPU
                                            if (isPpuDecoder && value == "LLVM Turbo") {
                                                safeSettingsSet(itemPath, "\"LLVM Recompiler (Legacy)\"")
                                                applyPpuLLVMTurbo()
                                                GeneralSettings.setValue("ppu_llvm_turbo", true)
                                                ppuTurboEnabled = true  // Update UI immediately
                                                itemObject.put("value", "LLVM Recompiler (Legacy)")
                                                itemValue = "LLVM Recompiler (Legacy)"
                                                return@SingleSelectionDialog
                                            }
                                            
                                            // Special handling for LLVM Turbo - SPU
                                            if (isSpuDecoder && value == "LLVM Turbo") {
                                                // Find the LLVM option in original variants
                                                val llvmOption = variants.find { it.contains("LLVM", ignoreCase = true) && it != "LLVM Turbo" } ?: "ASMJIT Recompiler"
                                                val internalLlvm = when (llvmOption) {
                                                    "LLVM 19" -> "LLVM Recompiler (Legacy)"
                                                    else -> llvmOption
                                                }
                                                safeSettingsSet(itemPath, "\"$internalLlvm\"")
                                                applySpuLLVMTurbo()
                                                GeneralSettings.setValue("spu_llvm_turbo", true)
                                                spuTurboEnabled = true  // Update UI immediately
                                                itemObject.put("value", internalLlvm)
                                                itemValue = internalLlvm
                                                return@SingleSelectionDialog
                                            }
                                            
                                            // Special handling for NCE - activates NCE Native!
                                            if (isPpuDecoder && value == "NCE") {
                                                // IMPORTANT: Must use LLVM 19 to compile PPU modules!
                                                // Interpreter skips PPU compilation entirely.
                                                safeSettingsSet(itemPath, "\"LLVM Recompiler (Legacy)\"")
                                                // NCE mode - native function not available, save settings only
                                                // safeSetNCEMode(3)
                                                // Save NCE mode (use cached setter for performance)
                                                net.rpcsx.utils.GeneralSettings.nceMode = 3
                                                itemObject.put("value", "LLVM Recompiler (Legacy)")
                                                itemValue = "LLVM Recompiler (Legacy)"
                                                
                                                // Log NCE Native activation
                                                android.util.Log.i("RPCSX-NCE", "╔════════════════════════════════════════╗")
                                                android.util.Log.i("RPCSX-NCE", "║   NCE Native Activated!                ║")
                                                android.util.Log.i("RPCSX-NCE", "║   Your phone IS now PlayStation 3!    ║")
                                                android.util.Log.i("RPCSX-NCE", "║   Using LLVM 19 Backend                ║")
                                                android.util.Log.i("RPCSX-NCE", "╚════════════════════════════════════════╝")
                                                return@SingleSelectionDialog
                                            }
                                            
                                            // Map display names back to internal values
                                            val internalValue = when (value) {
                                                "LLVM 19" -> "LLVM Recompiler (Legacy)"
                                                "Interpreter" -> "Interpreter (Legacy)"
                                                else -> value
                                            }
                                            
                                            // When switching to non-Turbo mode, disable Turbo flag and update UI
                                            if (isPpuDecoder && value != "LLVM Turbo") {
                                                GeneralSettings.setValue("ppu_llvm_turbo", false)
                                                ppuTurboEnabled = false  // Update UI immediately
                                            }
                                            if (isSpuDecoder && value != "LLVM Turbo") {
                                                GeneralSettings.setValue("spu_llvm_turbo", false)
                                                spuTurboEnabled = false  // Update UI immediately
                                            }
                                            
                                            if (!safeSettingsSet(
                                                    itemPath, "\"" + internalValue + "\""
                                                )
                                            ) {
                                                AlertDialogQueue.showDialog(
                                                    context.getString(R.string.error),
                                                    context.getString(R.string.failed_to_assign_value, value, itemPath)
                                                )
                                            } else {
                                                try {
                                                    itemObject.put("value", internalValue)
                                                    itemValue = internalValue  // Use internalValue not value
                                                    
                                                    // Sync NCE mode when PPU Decoder changes (save to preferences only)
                                                    if (isPpuDecoder) {
                                                        val nceMode = when (internalValue) {
                                                            "LLVM Recompiler (Legacy)" -> 2  // LLVM 19
                                                            "Interpreter (Legacy)", "Interpreter" -> 1
                                                            else -> 0
                                                        }
                                                        // Native function not available
                                                        // safeSetNCEMode(nceMode)
                                                        try {
                                                            net.rpcsx.utils.GeneralSettings.nceMode = nceMode
                                                        } catch (e: Throwable) {
                                                            android.util.Log.e("Settings", "Failed to save NCE mode: ${e.message}")
                                                        }
                                                    }
                                                    // Clear requested-version markers when user picks a non-20.3 backend
                                                    try {
                                                        if (isPpuDecoder) GeneralSettings.setValue("ppu_requested_llvm_version", null)
                                                        if (isSpuDecoder) GeneralSettings.setValue("spu_requested_llvm_version", null)
                                                    } catch (_: Throwable) {}
                                                } catch (e: Throwable) {
                                                    android.util.Log.e("Settings", "Error updating PPU decoder: ${e.message}")
                                                }
                                            }
                                        },
                                        onLongClick = {
                                            AlertDialogQueue.showDialog(
                                                title = context.getString(R.string.reset_setting),
                                                message = context.getString(R.string.ask_if_reset_key, key),
                                                onConfirm = {
                                                    if (safeSettingsSet(
                                                            itemPath, "\"" + def + "\""
                                                        )
                                                    ) {
                                                        itemObject.put("value", def)
                                                        itemValue = def
                                                    } else {
                                                        AlertDialogQueue.showDialog(
                                                            context.getString(R.string.error),
                                                            context.getString(R.string.failed_to_reset_key, key)
                                                        )
                                                    }
                                                })
                                        })
                                }
                            }

                            "uint", "int" -> {
                                var max = 0L
                                var min = 0L
                                var initialItemValue = 0L
                                var def = 0L
                                try {
                                    initialItemValue = itemObject.getString("value").toLong()
                                    max = itemObject.getString("max").toLong()
                                    min = itemObject.getString("min").toLong()
                                    def = itemObject.getString("default").toLong()
                                } catch (e: Throwable) {
                                    e.printStackTrace()
                                }
                                var itemValue by remember { mutableLongStateOf(initialItemValue) }
                                if (min < max) {
                                    SliderPreference(
                                        value = itemValue.toFloat(),
                                        valueRange = min.toFloat()..max.toFloat(),
                                        title = key + if (itemValue == def) "" else " *",
                                        steps = (max - min).toInt() - 1,
                                        onValueChange = { value ->
                                            if (!safeSettingsSet(
                                                    itemPath, value.toLong().toString()
                                                )
                                            ) {
                                                AlertDialogQueue.showDialog(
                                                    context.getString(R.string.error),
                                                    context.getString(R.string.failed_to_assign_value, value.toString(), itemPath)
                                                )
                                            } else {
                                                itemObject.put(
                                                    "value", value.toLong().toString()
                                                )
                                                itemValue = value.toLong()
                                            }
                                        },
                                        valueContent = { PreferenceValue(text = itemValue.toString()) },
                                        onLongClick = {
                                            AlertDialogQueue.showDialog(
                                                title = context.getString(R.string.reset_setting),
                                                message = context.getString(R.string.ask_if_reset_key, key),
                                                onConfirm = {
                                                    if (safeSettingsSet(
                                                            itemPath, def.toString()
                                                        )
                                                    ) {
                                                        itemObject.put("value", def)
                                                        itemValue = def
                                                    } else {
                                                        AlertDialogQueue.showDialog(
                                                            context.getString(R.string.error),
                                                            context.getString(R.string.failed_to_reset_key, key)
                                                        )
                                                    }
                                                })
                                        })
                                }
                            }

                            "float" -> {
                                var itemValue by remember {
                                    mutableStateOf(
                                        itemObject.getString(
                                            "value"
                                        ).toDouble()
                                    )
                                }
                                val max = if (itemObject.has("max")) itemObject.getString("max")
                                    .toDouble() else 0.0
                                val min = if (itemObject.has("min")) itemObject.getString("min")
                                    .toDouble() else 0.0
                                val def =
                                    if (itemObject.has("default")) itemObject.getString("default")
                                        .toDouble() else 0.0

                                if (min < max) {
                                    SliderPreference(
                                        value = itemValue.toFloat(),
                                        valueRange = min.toFloat()..max.toFloat(),
                                        title = key + if (itemValue == def) "" else " *",
                                        steps = ceil(max - min).toInt() - 1,
                                        onValueChange = { value ->
                                            if (!safeSettingsSet(
                                                    itemPath, value.toString()
                                                )
                                            ) {
                                                AlertDialogQueue.showDialog(
                                                    context.getString(R.string.error),
                                                    context.getString(R.string.failed_to_assign_value, value.toString(), itemPath)
                                                )
                                            } else {
                                                itemObject.put("value", value.toDouble().toString())
                                                itemValue = value.toDouble()
                                            }
                                        },
                                        valueContent = { PreferenceValue(text = itemValue.toString()) },
                                        onLongClick = {
                                            AlertDialogQueue.showDialog(
                                                title = context.getString(R.string.reset_setting),
                                                message = context.getString(R.string.ask_if_reset_key, key),
                                                onConfirm = {
                                                    if (safeSettingsSet(
                                                            itemPath, def.toString()
                                                        )
                                                    ) {
                                                        itemObject.put("value", def)
                                                        itemValue = def
                                                    } else {
                                                        AlertDialogQueue.showDialog(
                                                            context.getString(R.string.error),
                                                            context.getString(R.string.failed_to_reset_key, key)
                                                        )
                                                    }
                                                })
                                        })
                                }
                            }

                            else -> {
                                Log.e("Main", "Unimplemented setting type $type")
                            }
                        }
                    }
                }
            }

            if (path.isEmpty()) {
                item(key = "install_dev_rpcsx") {
                    RegularPreference(
                        title = stringResource(R.string.install_custom_rpcsx_lib),
                        leadingIcon = null,
                        onClick = { 
                            // Use improved APK installer with fallback methods
                            installRpcsxLauncher.launch("*/*") 
                        }
                    )
                }
            }
        }
    }
}


@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(
    modifier: Modifier = Modifier,
    navigateBack: () -> Unit,
    navigateTo: (path: String) -> Unit,
    onRefresh: () -> Unit
) {
    val topBarScrollBehavior = TopAppBarDefaults.enterAlwaysScrollBehavior()
    val activeUser by remember { UserRepository.activeUser }

    Scaffold(
        modifier = Modifier
            .nestedScroll(topBarScrollBehavior.nestedScrollConnection)
            .then(modifier), topBar = {
            LargeTopAppBar(
                title = { Text(text = stringResource(R.string.settings), fontWeight = FontWeight.Medium) },
                scrollBehavior = topBarScrollBehavior,
                navigationIcon = {
                    IconButton(
                        onClick = navigateBack
                    ) {
                        Icon(imageVector = Icons.Filled.KeyboardArrowLeft, null)
                    }
                })
        }
    ) { contentPadding ->
        val context = LocalContext.current
        val configPicker = rememberLauncherForActivityResult(
            contract = ActivityResultContracts.OpenDocument(),
            onResult = { uri: Uri? ->
                uri?.let { 
                    if (FileUtil.importConfig(context, it))
                        onRefresh()
                }
            }
        )

        val configExporter = rememberLauncherForActivityResult(
            contract = ActivityResultContracts.CreateDocument("application/x-yaml"),
            onResult = { uri: Uri? ->
                uri?.let { FileUtil.exportConfig(context, it) }
            }
        )
        
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(contentPadding),
        ) {
            item {
                Spacer(modifier = Modifier.height(16.dp))
            }

            item(
                key = "internal_directory"
            ) {
                HomePreference(
                    title = stringResource(R.string.view_internal_dir),
                    icon = { PreferenceIcon(icon = painterResource(R.drawable.ic_folder)) },
                    description = stringResource(R.string.view_internal_dir_description),
                    onClick = {
                        if (!FileUtil.launchInternalDir(context)) {
                            AlertDialogQueue.showDialog(
                                context.getString(R.string.failed_to_view_internal_dir),
                                context.getString(R.string.no_activity_to_handle_action)
                            )
                        }
                    }
                )
            }

            item(
                key = "users"
            ) {
                HomePreference(
                    title = stringResource(R.string.users),
                    description = "${stringResource(R.string.active_user)}: ${UserRepository.getUsername(activeUser)}",
                    icon = {
                        PreferenceIcon(icon = Icons.Default.Person)
                    },
                    onClick = {
                        navigateTo("users")
                    }
                )
            }

            item(key = "update_channels") {
                HomePreference(
                    title = stringResource(R.string.download_channels),
                    icon = { PreferenceIcon(icon = painterResource(R.drawable.ic_cloud_download)) },
                    description = "",
                    onClick = {
                        navigateTo("update_channels")
                    }
                )
            }

            item(key = "advanced_settings") {
                HomePreference(
                    title = stringResource(R.string.advanced_settings),
                    icon = { Icon(painterResource(R.drawable.tune), null) },
                    description = stringResource(R.string.advanced_settings_description),
                    onClick = {
                        navigateTo("settings@@$")
                    },
                    onLongClick = {
                        AlertDialogQueue.showDialog(
                            title = context.getString(R.string.manage_settings),
                            confirmText = context.getString(R.string.export),
                            dismissText = context.getString(R.string.import_),
                            onDismiss = {
                                configPicker.launch(arrayOf("*/*"))
                            },
                            onConfirm = {
                                configExporter.launch("config.yml")
                            }
                        )
                    }
                )
            }

            item(
                key = "custom_driver"
            ) {
                HomePreference(
                    title = stringResource(R.string.custom_driver),
                    icon = { Icon(painterResource(R.drawable.memory), contentDescription = null) },
                    description = stringResource(R.string.custom_driver_description),
                    onClick = {
                        try {
                            if (net.rpcsx.utils.safeNativeCall { RPCSX.instance.supportsCustomDriverLoading() } == true) {
                                navigateTo("drivers")
                            } else {
                                AlertDialogQueue.showDialog(
                                    title = context.getString(R.string.custom_driver_not_supported),
                                    message = context.getString(R.string.custom_driver_not_supported_description),
                                    confirmText = context.getString(R.string.close),
                                    dismissText = ""
                                )
                            }
                        } catch (e: Throwable) {
                            AlertDialogQueue.showDialog(
                                title = context.getString(R.string.custom_driver_not_supported),
                                message = "Error: ${e.message}",
                                confirmText = context.getString(R.string.close),
                                dismissText = ""
                            )
                        }
                    }  
                )
            }

            item(key = "controls") {
                HomePreference(
                    title = stringResource(R.string.controls),
                    icon = { Icon(painterResource(R.drawable.gamepad), null) },
                    description = stringResource(R.string.controls_description),
                    onClick = { navigateTo("controls") }
                )
            }

            item(key = "share_logs") {
                HomePreference(
                    title = stringResource(R.string.share_log),
                    icon = { Icon(imageVector = Icons.Default.Share, contentDescription = null) },
                    description = stringResource(R.string.share_log_description),
                    onClick = {
                        val file = DocumentFile.fromSingleUri(
                            context, DocumentsContract.buildDocumentUri(
                                AppDataDocumentProvider.AUTHORITY,
                                "${AppDataDocumentProvider.ROOT_ID}/cache/RPCSX${if (RPCSX.lastPlayedGame.isNotEmpty()) "" else ".old"}.log"
                            )
                        )

                        if (file != null && file.exists() && file.length() != 0L) {
                            val intent = Intent(Intent.ACTION_SEND).apply {
                                setDataAndType(file.uri, "text/plain")
                                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                                putExtra(Intent.EXTRA_STREAM, file.uri)
                            }
                            context.startActivity(Intent.createChooser(intent, context.getString(R.string.share_log)))
                        } else {
                            Toast.makeText(context, context.getString(R.string.log_not_found), Toast.LENGTH_SHORT).show()
                        }
                    }
                )
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ControllerSettings(
    modifier: Modifier = Modifier,
    navigateBack: () -> Unit
) {
    val topBarScrollBehavior = TopAppBarDefaults.enterAlwaysScrollBehavior()
    Scaffold(
        modifier = Modifier
            .nestedScroll(topBarScrollBehavior.nestedScrollConnection)
            .then(modifier),
        topBar = {
            LargeTopAppBar(
                title = { Text(text = stringResource(R.string.controls), fontWeight = FontWeight.Medium) },
                scrollBehavior = topBarScrollBehavior,
                navigationIcon = {
                    IconButton(
                        onClick = navigateBack
                    ) {
                        Icon(imageVector = Icons.Filled.KeyboardArrowLeft, null)
                    }
                }
            )
        }
    ) { contentPadding ->
        val context = LocalContext.current
        val inputBindings = remember {
            mutableStateMapOf<Int, Pair<Int, Int>>().apply {
                putAll(InputBindingPrefs.loadBindings())
            }
        }

        var showDialog by remember { mutableStateOf<Boolean>(false) }
        var currentInput by remember { mutableStateOf<Int>(-1) }
        var currentInputName by remember { mutableStateOf<String>("") }
        val requester = remember { FocusRequester() }

        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(contentPadding),
        ) {
            item {
                Spacer(modifier = Modifier.height(16.dp))
            }

            item {
                PreferenceHeader(stringResource(R.string.gamepad_overlay))
            }

            item {
                var itemValue by remember {
                    mutableStateOf(
                        GeneralSettings["haptic_feedback"] as Boolean? ?: true
                    )
                }
                val def = true
                SwitchPreference(
                    checked = itemValue,
                    title = stringResource(R.string.enable_haptic_feedback) + if (itemValue == def) "" else " *",
                    leadingIcon = null,
                    onClick = { value ->
                        GeneralSettings.setValue("haptic_feedback", value)
                        itemValue = value
                    }
                )
            }

            item {
                HorizontalDivider()
            }

            item {
                PreferenceHeader(stringResource(R.string.key_mappings))
            }

            inputBindings.toList()
                .sortedBy { (_, value) ->
                    val name = InputBindingPrefs.rpcsxKeyCodeToString(value.first, value.second)
                    InputBindingPrefs.defaultBindings.values.indexOfFirst { defValue ->
                        InputBindingPrefs.rpcsxKeyCodeToString(
                            defValue.first,
                            defValue.second
                        ) == name
                    }
                }
                .forEach { binding ->
                    item {
                        RegularPreference(
                            title = InputBindingPrefs.rpcsxKeyCodeToString(
                                binding.second.first,
                                binding.second.second
                            ),
                            value = {
                                PreferenceValue(
                                    if (binding.first.toString().length > 4) stringResource(R.string.none)
                                    else KeyEvent.keyCodeToString(binding.first)
                                )
                            },
                            onClick = {
                                currentInput = binding.first
                                currentInputName = InputBindingPrefs.rpcsxKeyCodeToString(
                                    binding.second.first,
                                    binding.second.second
                                )
                                showDialog = true
                            }
                        )
                    }
                }
        }

        if (showDialog) {
            InputBindingDialog(
                onReset = {
                    InputBindingPrefs.defaultBindings.forEach { it ->
                        if (InputBindingPrefs.rpcsxKeyCodeToString(
                                it.value.first,
                                it.value.second
                            ) == currentInputName
                        ) {
                            inputBindings[currentInput]?.let { value ->
                                inputBindings.remove(currentInput)
                                inputBindings[it.key] = value
                            }
                            InputBindingPrefs.saveBindings(inputBindings.toMap())
                        }
                    }
                },
                onDismissRequest = { showDialog = false },
                modifier = Modifier
                    .onKeyEvent { keyEvent ->
                        if (keyEvent.type == KeyEventType.KeyDown) {
                            if (showDialog) {
                                if (inputBindings.containsKey(keyEvent.nativeKeyEvent.keyCode)) {
                                    inputBindings[keyEvent.nativeKeyEvent.keyCode]?.let { value ->
                                        inputBindings.remove(keyEvent.nativeKeyEvent.keyCode)
                                        inputBindings[(10000..99999).random()] = value
                                    }
                                }
                                inputBindings[currentInput]?.let { value ->
                                    inputBindings.remove(currentInput)
                                    inputBindings[keyEvent.nativeKeyEvent.keyCode] = value
                                }
                                InputBindingPrefs.saveBindings(inputBindings.toMap())
                                showDialog = false
                                true
                            } else false
                        } else false
                    }
                    .focusRequester(requester)
                    .focusable()

            )

            LaunchedEffect(showDialog) {
                requester.requestFocus()
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun InputBindingDialog(
    modifier: Modifier = Modifier,
    onReset: () -> Unit = {},
    onDismissRequest: () -> Unit = {}
) {
    ModalBottomSheet(
        onDismissRequest = onDismissRequest
    ) {
        Column(
            modifier = modifier
                .fillMaxWidth()
                .padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(
                text = stringResource(R.string.perform_input),
                style = MaterialTheme.typography.titleMedium,
                modifier = Modifier.align(Alignment.CenterHorizontally)
            )

            Spacer(modifier = Modifier.height(10.dp))

            Box(
                contentAlignment = Alignment.Center,
                modifier = Modifier.size(75.dp)
            ) {
                ButtonMappingAnim()
            }

            Spacer(modifier = Modifier.height(10.dp))

            Button(
                onClick = onReset,
                modifier = Modifier.align(Alignment.End)
            ) {
                Text(stringResource(R.string.reset))
            }
        }
    }
}

@Composable
fun ButtonMappingAnim() {
    val infiniteTransition = rememberInfiniteTransition()

    val scaleX by infiniteTransition.animateFloat(
        initialValue = 1.2f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(
            animation = tween(durationMillis = 568),
            repeatMode = RepeatMode.Reverse
        )
    )

    val scaleY by infiniteTransition.animateFloat(
        initialValue = 1.2f,
        targetValue = 1f,
        animationSpec = infiniteRepeatable(
            animation = tween(durationMillis = 568),
            repeatMode = RepeatMode.Reverse
        )
    )

    Image(
        painter = painterResource(id = R.drawable.button_mapping),
        contentDescription = null,
        modifier = Modifier
            .graphicsLayer(
                scaleX = scaleX,
                scaleY = scaleY
            )
            .fillMaxSize()
    )
}

@Preview
@Composable
private fun SettingsScreenPreview() {
    ComposePreview {
//        SettingsScreen {}
    }
}
