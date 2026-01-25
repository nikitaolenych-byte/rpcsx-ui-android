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
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowLeft
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Share
import androidx.compose.material3.Button
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
import androidx.compose.runtime.mutableDoubleStateOf
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
import org.json.JSONObject
import java.io.File
import kotlin.math.ceil

// Safe wrapper for RPCSX native calls
private fun safeSettingsSet(path: String, value: String): Boolean {
    // Check if library is loaded
    if (RPCSX.activeLibrary.value == null) {
        Log.w("Settings", "Cannot set $path: RPCSX library not loaded")
        return false
    }
    return try {
        val result = RPCSX.instance.settingsSet(path, value)
        if (!result) {
            Log.w("Settings", "settingsSet returned false for $path = $value")
        }
        result
    } catch (e: Throwable) {
        Log.e("Settings", "Error setting $path to $value: ${e.message}")
        false
    }
}

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

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AdvancedSettingsScreen(
    modifier: Modifier = Modifier,
    navigateBack: () -> Unit,
    navigateTo: (path: String) -> Unit,
    settings: JSONObject,
    path: String = ""
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

                    if (RPCSX.instance.getLibraryVersion(target.path) != null) {
                        RpcsxUpdater.installUpdate(context, target)
                    }
                }
            }
        }

    val topBarScrollBehavior = TopAppBarDefaults.enterAlwaysScrollBehavior()
    Scaffold(
        modifier = Modifier
            .nestedScroll(topBarScrollBehavior.nestedScrollConnection)
            .then(modifier), topBar = {
            val titlePath = path.replace("@@", " / ").removePrefix(" / ")
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
                        text = titlePath.ifEmpty { stringResource(R.string.advanced_settings) },
                        fontWeight = FontWeight.Medium
                    )
                }
            }, scrollBehavior = topBarScrollBehavior, navigationIcon = {
                IconButton(onClick = navigateBack) {
                    Icon(
                        imageVector = Icons.AutoMirrored.Default.KeyboardArrowLeft,
                        contentDescription = null
                    )
                }
            }, actions = {
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
                                for (i in 0..<variantsJson.length()) {
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
                                
                                // Reactive state for turbo flags - updates UI immediately
                                var ppuTurboEnabled by remember { mutableStateOf(GeneralSettings["ppu_llvm_turbo"] as? Boolean ?: false) }
                                var spuTurboEnabled by remember { mutableStateOf(GeneralSettings["spu_llvm_turbo"] as? Boolean ?: false) }
                                
                                // Check if NCE mode is active (use cached value for performance)
                                val savedNceMode = net.rpcsx.utils.GeneralSettings.nceMode
                                
                                // Compute display value based on current state
                                val displayValue = when {
                                    isPpuDecoder && savedNceMode == 3 -> "NCE"
                                    isPpuDecoder && ppuTurboEnabled && itemValue == "LLVM Recompiler (Legacy)" -> "LLVM Turbo"
                                    isPpuDecoder && itemValue == "LLVM Recompiler (Legacy)" -> "LLVM 19"
                                    isPpuDecoder && itemValue == "Interpreter (Legacy)" -> "Interpreter"
                                    isSpuDecoder && spuTurboEnabled && itemValue.contains("LLVM", ignoreCase = true) -> "LLVM Turbo"
                                    else -> itemValue
                                }

                                Column {
                                    SingleSelectionDialog(
                                        currentValue = if (displayValue in variants) displayValue else variants[0],
                                        values = variants,
                                        icon = null,
                                        title = key + if (itemValue == def) "" else " *",
                                        onValueChange = { value ->
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
                                    mutableDoubleStateOf(
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
                        Icon(imageVector = Icons.AutoMirrored.Default.KeyboardArrowLeft, null)
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
                            if (RPCSX.instance.supportsCustomDriverLoading()) {
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

            // ARM Optimization Settings
            item(key = "asimd_neon") {
                var asimdEnabled by remember { mutableStateOf(GeneralSettings["asimd_neon_enabled"] as? Boolean ?: true) }
                SwitchPreference(
                    checked = asimdEnabled,
                    title = "ASIMD (NEON)",
                    subtitle = { PreferenceSubtitle(subtitle = "Advanced SIMD for Cell PPU/SPU emulation") },
                    onClick = { enabled ->
                        asimdEnabled = enabled
                        GeneralSettings.setValue("asimd_neon_enabled", enabled)
                        // Apply to emulator settings
                        safeSettingsSet("Core@@Use ASIMD", if (enabled) "true" else "false")
                        safeSettingsSet("Core@@Preferred SIMD", if (enabled) "\"NEON\"" else "\"auto\"")
                    }
                )
            }

            item(key = "sve2_enabled") {
                var sve2Enabled by remember { mutableStateOf(GeneralSettings["sve2_enabled"] as? Boolean ?: false) }
                SwitchPreference(
                    checked = sve2Enabled,
                    title = "SVE2 (Scalable Vector Extension 2)",
                    subtitle = { PreferenceSubtitle(subtitle = "SVE2 for Snapdragon 8 Gen 3/4/5") },
                    onClick = { enabled ->
                        sve2Enabled = enabled
                        GeneralSettings.setValue("sve2_enabled", enabled)
                        // Apply to emulator settings
                        safeSettingsSet("Core@@Use SVE2", if (enabled) "true" else "false")
                        safeSettingsSet("Core@@SPU SVE2 Acceleration", if (enabled) "true" else "false")
                    }
                )
            }

            item(key = "zram_swap") {
                var zramEnabled by remember { mutableStateOf(GeneralSettings["zram_enabled"] as? Boolean ?: true) }
                SwitchPreference(
                    checked = zramEnabled,
                    title = "zRAM / Swap",
                    subtitle = { PreferenceSubtitle(subtitle = "Compressed virtual memory (4-8GB for PS3)") },
                    onClick = { enabled ->
                        zramEnabled = enabled
                        GeneralSettings.setValue("zram_enabled", enabled)
                        // Apply memory optimization settings
                        safeSettingsSet("Core@@Memory Management", if (enabled) "\"aggressive\"" else "\"default\"")
                        safeSettingsSet("Core@@Use Large Pages", if (enabled) "true" else "false")
                    }
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
                        Icon(imageVector = Icons.AutoMirrored.Default.KeyboardArrowLeft, null)
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
