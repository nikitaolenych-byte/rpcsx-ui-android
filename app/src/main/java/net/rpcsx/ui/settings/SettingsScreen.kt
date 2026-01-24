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
    return try {
        RPCSX.instance.settingsSet(path, value)
    } catch (e: Throwable) {
        Log.e("Settings", "Error setting $path to $value: ${e.message}")
        false
    }
}

private fun safeSetNCEMode(mode: Int) {
    try {
        RPCSX.instance.setNCEMode(mode)
    } catch (e: Throwable) {
        Log.e("Settings", "Error setting NCE mode to $mode: ${e.message}")
    }
}

private fun applyPpuLLVMTurbo(): Boolean {
    val updates = listOf(
        "Core@@PPU LLVM Greedy Mode" to "true",
        "Core@@LLVM Precompilation" to "true",
        "Core@@Set DAZ and FTZ" to "true",
        "Core@@Thread Scheduler Mode" to "\"alt\"",
        "Core@@PPU LLVM Java Mode Handling" to "true",
        "Core@@PPU Accurate Non-Java Mode" to "false",
        "Core@@PPU Set Saturation Bit" to "false",
        "Core@@PPU Set FPCC Bits" to "false",
        "Core@@PPU Fixup Vector NaN Values" to "false",
        "Core@@PPU Accurate Vector NaN Values" to "false",
        "Core@@Accurate Cache Line Stores" to "false",
        "Core@@Accurate PPU 128-byte Reservation Op Max Length" to "0",
        "Core@@Use Accurate DFMA" to "false"
    )

    return updates.all { (path, value) -> safeSettingsSet(path, value) }
}

private fun applySpuLLVMTurbo(): Boolean {
    val updates = listOf(
        "Core@@SPU Cache" to "true",
        "Core@@SPU Verification" to "false",
        "Core@@Precise SPU Verification" to "false",
        "Core@@SPU Accurate DMA" to "false",
        "Core@@SPU Accurate Reservations" to "false",
        "Core@@SPU Block Size" to "\"giga\"",
        "Core@@Preferred SPU Threads" to "6",
        "Core@@Max SPURS Threads" to "6",
        "Core@@SPU loop detection" to "true",
        "Core@@SPU delay penalty" to "0",
        "Core@@XFloat Accuracy" to "\"approximate\""
    )

    return updates.all { (path, value) -> safeSettingsSet(path, value) }
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
                                
                                // Check if NCE mode is active (use cached value for performance)
                                // If NCE mode = 3, show "NCE" even if underlying decoder is LLVM 19
                                val savedNceMode = net.rpcsx.utils.GeneralSettings.nceMode
                                val displayValue = when {
                                    isPpuDecoder && savedNceMode == 3 -> "NCE"
                                    isPpuDecoder && itemValue == "LLVM Recompiler (Legacy)" -> "LLVM 19"
                                    isPpuDecoder && itemValue == "Interpreter (Legacy)" -> "Interpreter"
                                    else -> itemValue
                                }

                                val isPpuLLVMSelected = isPpuDecoder && itemValue == "LLVM Recompiler (Legacy)"
                                val isSpuLLVMSelected = isSpuDecoder && itemValue.contains("LLVM", ignoreCase = true)
                                val ppuTurboKey = "ppu_llvm_turbo"
                                val spuTurboKey = "spu_llvm_turbo"
                                var ppuTurboEnabled by remember {
                                    mutableStateOf(GeneralSettings[ppuTurboKey] as? Boolean ?: false)
                                }
                                var spuTurboEnabled by remember {
                                    mutableStateOf(GeneralSettings[spuTurboKey] as? Boolean ?: false)
                                }

                                Column {
                                    SingleSelectionDialog(
                                        currentValue = if (displayValue in variants) displayValue else variants[0],
                                        values = variants,
                                        icon = null,
                                        title = key + if (itemValue == def) "" else " *",
                                        onValueChange = { value ->
                                            // Special handling for NCE - activates NCE Native!
                                            if (isPpuDecoder && value == "NCE") {
                                                // IMPORTANT: Must use LLVM 19 to compile PPU modules!
                                                // Interpreter skips PPU compilation entirely.
                                                safeSettingsSet(itemPath, "\"LLVM Recompiler (Legacy)\"")
                                                // Activate NCE Native - Your phone IS PlayStation 3!
                                                safeSetNCEMode(3)
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
                                                    
                                                    // Sync NCE mode when PPU Decoder changes
                                                    if (isPpuDecoder) {
                                                        val nceMode = when (internalValue) {
                                                            "LLVM Recompiler (Legacy)" -> 2  // LLVM 19
                                                            "Interpreter (Legacy)", "Interpreter" -> 1
                                                            else -> 0
                                                        }
                                                        safeSetNCEMode(nceMode)
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

                                    if (isPpuDecoder && isPpuLLVMSelected) {
                                        SwitchPreference(
                                            checked = ppuTurboEnabled,
                                            title = "PPU LLVM Turbo",
                                            subtitle = {
                                                Text("Aggressive JIT, NEON fusion, speculative execution, hot-path locking, unsafe fast-math, unrolling, prefetch")
                                            },
                                            onClick = { enabled ->
                                                if (enabled) {
                                                    if (!applyPpuLLVMTurbo()) {
                                                        Toast.makeText(context, "Failed to apply PPU LLVM Turbo", Toast.LENGTH_SHORT).show()
                                                        return@SwitchPreference
                                                    }
                                                }
                                                ppuTurboEnabled = enabled
                                                GeneralSettings.setValue(ppuTurboKey, enabled)
                                                Toast.makeText(
                                                    context,
                                                    if (enabled) "PPU LLVM Turbo enabled" else "PPU LLVM Turbo disabled",
                                                    Toast.LENGTH_SHORT
                                                ).show()
                                            }
                                        )
                                    }

                                    if (isSpuDecoder && isSpuLLVMSelected) {
                                        SwitchPreference(
                                            checked = spuTurboEnabled,
                                            title = "SPU LLVM Turbo",
                                            subtitle = {
                                                Text("NEON mapping, speculative DMA, hot-path caching, approximate math, unrolling, prefetch, relaxed sync")
                                            },
                                            onClick = { enabled ->
                                                if (enabled) {
                                                    if (!applySpuLLVMTurbo()) {
                                                        Toast.makeText(context, "Failed to apply SPU LLVM Turbo", Toast.LENGTH_SHORT).show()
                                                        return@SwitchPreference
                                                    }
                                                }
                                                spuTurboEnabled = enabled
                                                GeneralSettings.setValue(spuTurboKey, enabled)
                                                Toast.makeText(
                                                    context,
                                                    if (enabled) "SPU LLVM Turbo enabled" else "SPU LLVM Turbo disabled",
                                                    Toast.LENGTH_SHORT
                                                ).show()
                                            }
                                        )
                                    }
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
            
            // RSX Video Settings - show when in Video, GPU, Graphics section or root level
            // Debug log to see actual path
            Log.d("RSX_Settings", "Current path: '$path'")
            val showRsxSettings = path.contains("Video", ignoreCase = true) || 
                                   path.contains("GPU", ignoreCase = true) ||
                                   path.contains("Graphics", ignoreCase = true) ||
                                   path.contains("Render", ignoreCase = true) ||
                                   path.contains("Display", ignoreCase = true) ||
                                   path == "@@$" ||  // Root of advanced settings
                                   path.isEmpty()    // Also show at root
            if (showRsxSettings) {
                item(key = "rsx_settings_block") {
                    var rsxEnabled by remember { 
                        mutableStateOf(
                            try { RPCSX.instance.rsxIsRunning() } catch (e: Throwable) { true }
                        ) 
                    }
                    var rsxThreadCount by remember {
                        mutableStateOf(
                            try { RPCSX.instance.rsxGetThreadCount().toFloat() } catch (e: Throwable) { 8f }
                        )
                    }
                    var rsxResolutionScale by remember {
                        mutableStateOf((GeneralSettings["rsx_resolution_scale"] as? Int ?: 50).toFloat())
                    }
                    var rsxVsyncEnabled by remember { mutableStateOf(GeneralSettings["rsx_vsync"] as? Boolean ?: false) }
                    var rsxFrameLimitEnabled by remember { mutableStateOf(GeneralSettings["rsx_frame_limit"] as? Boolean ?: false) }

                    Column {
                        PreferenceHeader(text = stringResource(R.string.rsx_video_settings))

                        RegularPreference(
                            title = "Max Performance",
                            subtitle = { Text("Enables NCE, max RSX threads, no VSync/limit, 50% resolution") },
                            onClick = {
                                try {
                                    // PPU Decoder + NCE
                                    safeSettingsSet("Core@@PPU Decoder", "\"LLVM Recompiler (Legacy)\"")
                                    safeSetNCEMode(3)
                                    try {
                                        GeneralSettings.nceMode = 3
                                    } catch (e: Throwable) {
                                        Log.e("Settings", "Failed to save NCE mode: ${e.message}")
                                    }

                                    // LLVM performance tweaks
                                    safeSettingsSet("Core@@PPU LLVM Greedy Mode", "true")
                                    safeSettingsSet("Core@@LLVM Precompilation", "true")
                                    safeSettingsSet("Core@@Set DAZ and FTZ", "true")
                                    safeSettingsSet("Core@@Max LLVM Compile Threads", "16")

                                    // RSX engine
                                    if (!rsxEnabled) {
                                        try {
                                            if (RPCSX.instance.rsxStart()) {
                                                rsxEnabled = true
                                            }
                                        } catch (e: Throwable) {
                                            Log.e("RSX", "Failed to start RSX: ${e.message}")
                                        }
                                    }

                                    val maxThreads = 8
                                    try {
                                        RPCSX.instance.rsxSetThreadCount(maxThreads)
                                        rsxThreadCount = maxThreads.toFloat()
                                        GeneralSettings.setValue("rsx_thread_count", maxThreads)
                                    } catch (e: Throwable) {
                                        Log.e("RSX", "Failed to set thread count: ${e.message}")
                                    }

                                    rsxResolutionScale = 50f
                                    GeneralSettings.setValue("rsx_resolution_scale", 50)
                                    rsxVsyncEnabled = false
                                    GeneralSettings.setValue("rsx_vsync", false)
                                    rsxFrameLimitEnabled = false
                                    GeneralSettings.setValue("rsx_frame_limit", false)
                                    GeneralSettings.setValue("auto_max_performance", true)
                                    GeneralSettings.setValue("max_perf_defaults_applied", true)

                                    Toast.makeText(context, "Max performance enabled", Toast.LENGTH_SHORT).show()
                                } catch (e: Throwable) {
                                    Toast.makeText(context, "Failed to apply performance mode", Toast.LENGTH_SHORT).show()
                                }
                            }
                        )

                        SwitchPreference(
                            checked = rsxEnabled,
                            title = stringResource(R.string.rsx_enabled),
                            subtitle = { Text(stringResource(R.string.rsx_enabled_description)) },
                            onClick = { enabled ->
                                try {
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
                                    GeneralSettings.setValue("rsx_enabled", enabled)
                                } catch (e: Throwable) {
                                    Toast.makeText(context, "RSX not available: ${e.message}", Toast.LENGTH_SHORT).show()
                                }
                            }
                        )
                        
                        SliderPreference(
                            value = rsxThreadCount,
                            valueRange = 1f..8f,
                            title = stringResource(R.string.rsx_thread_count),
                            steps = 6,
                            onValueChange = { value ->
                                try {
                                    val count = value.toInt()
                                    RPCSX.instance.rsxSetThreadCount(count)
                                    rsxThreadCount = value
                                    GeneralSettings.setValue("rsx_thread_count", count)
                                } catch (e: Throwable) {
                                    Log.e("RSX", "Failed to set thread count: ${e.message}")
                                }
                            },
                            valueContent = {
                                Text(text = "${rsxThreadCount.toInt()} threads")
                            }
                        )
                        
                        val rsxRunning = try { RPCSX.instance.rsxIsRunning() } catch (e: Throwable) { false }
                        val fps = if (rsxRunning) { try { RPCSX.instance.getRSXFPS() } catch (e: Throwable) { 0 } } else 0
                        RegularPreference(
                            title = stringResource(R.string.rsx_stats),
                            subtitle = { Text(if (rsxRunning) "FPS: $fps" else stringResource(R.string.rsx_status_off)) },
                            onClick = {
                                try {
                                    val stats = RPCSX.instance.rsxGetStats()
                                    Toast.makeText(context, stats, Toast.LENGTH_LONG).show()
                                } catch (e: Throwable) {
                                    Toast.makeText(context, "RSX stats not available", Toast.LENGTH_SHORT).show()
                                }
                            }
                        )
                        
                        SliderPreference(
                            value = rsxResolutionScale,
                            valueRange = 50f..200f,
                            title = "Resolution Scale",
                            steps = 5,
                            onValueChange = { value ->
                                rsxResolutionScale = value
                                GeneralSettings.setValue("rsx_resolution_scale", value.toInt())
                            },
                            valueContent = {
                                Text(text = "${rsxResolutionScale.toInt()}%")
                            }
                        )
                        
                        SwitchPreference(
                            checked = rsxVsyncEnabled,
                            title = "VSync",
                            subtitle = { Text("Synchronize frame rate with display refresh") },
                            onClick = { enabled ->
                                rsxVsyncEnabled = enabled
                                GeneralSettings.setValue("rsx_vsync", enabled)
                            }
                        )
                        
                        SwitchPreference(
                            checked = rsxFrameLimitEnabled,
                            title = "Frame Limit",
                            subtitle = { Text("Limit frame rate to 60 FPS") },
                            onClick = { enabled ->
                                rsxFrameLimitEnabled = enabled
                                GeneralSettings.setValue("rsx_frame_limit", enabled)
                            }
                        )
                    }
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
