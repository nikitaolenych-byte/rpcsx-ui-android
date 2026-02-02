package net.rpcsx.ui.patches

import android.content.Intent
import android.net.Uri
import android.util.Log
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.vectorResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import net.rpcsx.R

/**
 * RPCSX Patch Manager Screen
 * Ported from RPCS3 patch system with full RPCS3 patch.yml support
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun PatchManagerScreen(
    navigateBack: () -> Unit,
    gameId: String = "",
    gameName: String = ""
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    
    // UI State
    var isLoading by remember { mutableStateOf(false) }
    var statusMessage by remember { mutableStateOf<String?>(null) }
    var searchQuery by remember { mutableStateOf("") }
    var patchGroups by remember { mutableStateOf<List<PatchManager.PatchGroup>>(emptyList()) }
    var expandedGroups by remember { mutableStateOf<Set<String>>(setOf()) }
    var showDownloadDialog by remember { mutableStateOf(false) }
    var downloadProgress by remember { mutableStateOf(0f) }
    
    // Load patches on first composition
    LaunchedEffect(Unit) {
        patchGroups = PatchManager.loadPatchesFromCache(context)
        // Filter for specific game if gameId provided
        if (gameId.isNotBlank()) {
            val filtered = patchGroups.filter { it.gameId.contains(gameId, ignoreCase = true) }
            if (filtered.isNotEmpty()) {
                patchGroups = filtered
            }
        }
    }
    
    // Filter patches based on search
    val filteredGroups = remember(patchGroups, searchQuery) {
        if (searchQuery.isBlank()) {
            patchGroups
        } else {
            PatchManager.searchPatches(context, searchQuery)
        }
    }
    
    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Column {
                        Text("Patch Manager", fontSize = 18.sp)
                        if (gameName.isNotBlank()) {
                            Text(gameName, fontSize = 12.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                    }
                },
                navigationIcon = {
                    IconButton(onClick = navigateBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
                actions = {
                    IconButton(onClick = { showDownloadDialog = true }) {
                        Icon(ImageVector.vectorResource(R.drawable.ic_cloud_download), contentDescription = "Download Patches")
                    }
                    IconButton(onClick = {
                        scope.launch {
                            isLoading = true
                            statusMessage = "Refreshing..."
                            patchGroups = PatchManager.loadPatchesFromCache(context)
                            if (gameId.isNotBlank()) {
                                val filtered = patchGroups.filter { it.gameId.contains(gameId, ignoreCase = true) }
                                if (filtered.isNotEmpty()) patchGroups = filtered
                            }
                            isLoading = false
                            statusMessage = "Loaded ${patchGroups.sumOf { it.patches.size }} patches"
                        }
                    }) {
                        Icon(Icons.Default.Refresh, contentDescription = "Refresh")
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.primaryContainer
                )
            )
        }
    ) { paddingValues ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues)
        ) {
            // Search bar
            OutlinedTextField(
                value = searchQuery,
                onValueChange = { searchQuery = it },
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(16.dp),
                placeholder = { Text("Search patches by name, game ID, or author...") },
                leadingIcon = { Icon(Icons.Default.Search, contentDescription = null) },
                singleLine = true,
                shape = RoundedCornerShape(24.dp)
            )
            
            // Status message
            if (statusMessage != null) {
                Text(
                    text = statusMessage!!,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 16.dp, vertical = 4.dp),
                    color = MaterialTheme.colorScheme.primary,
                    fontSize = 12.sp
                )
            }
            
            // Loading indicator
            if (isLoading) {
                LinearProgressIndicator(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 16.dp),
                    progress = { downloadProgress }
                )
            }
            
            // Patch list
            if (filteredGroups.isEmpty() && !isLoading) {
                Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.Center
                ) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Text("No patches found", fontSize = 16.sp)
                        Text(
                            "Tap the download button to get patches from RPCS3",
                            fontSize = 12.sp,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                        Spacer(modifier = Modifier.height(16.dp))
                        Button(onClick = { showDownloadDialog = true }) {
                            Icon(ImageVector.vectorResource(R.drawable.ic_cloud_download), contentDescription = null)
                            Spacer(modifier = Modifier.width(8.dp))
                            Text("Download Patches")
                        }
                    }
                }
            } else {
                LazyColumn(
                    modifier = Modifier.fillMaxSize(),
                    contentPadding = PaddingValues(16.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    items(filteredGroups) { group ->
                        PatchGroupCard(
                            group = group,
                            isExpanded = expandedGroups.contains(group.gameId),
                            onExpandToggle = {
                                expandedGroups = if (expandedGroups.contains(group.gameId)) {
                                    expandedGroups - group.gameId
                                } else {
                                    expandedGroups + group.gameId
                                }
                            },
                            onPatchToggle = { patch ->
                                PatchManager.togglePatch(context, patch.id, !PatchManager.isPatchEnabled(context, patch.id))
                                // Refresh list
                                patchGroups = PatchManager.loadPatchesFromCache(context)
                                if (gameId.isNotBlank()) {
                                    val filtered = patchGroups.filter { it.gameId.contains(gameId, ignoreCase = true) }
                                    if (filtered.isNotEmpty()) patchGroups = filtered
                                }
                            },
                            isPatchEnabled = { patch -> PatchManager.isPatchEnabled(context, patch.id) }
                        )
                    }
                }
            }
        }
    }
    
    // Download dialog
    if (showDownloadDialog) {
        AlertDialog(
            onDismissRequest = { showDownloadDialog = false },
            title = { Text("Download Patches") },
            text = {
                Column {
                    Text("Download the latest patches from RPCS3 repository?")
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(
                        "This will download the official RPCS3 patch database containing patches for many PS3 games.",
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            },
            confirmButton = {
                Button(
                    onClick = {
                        showDownloadDialog = false
                        scope.launch {
                            isLoading = true
                            statusMessage = "Downloading patches from RPCS3..."
                            downloadProgress = 0f
                            
                            val result = withContext(Dispatchers.IO) {
                                PatchManager.downloadPatches(context) { progress, message ->
                                    downloadProgress = progress / 100f
                                    statusMessage = message
                                }
                            }
                            
                            if (result.isSuccess) {
                                patchGroups = PatchManager.loadPatchesFromCache(context)
                                if (gameId.isNotBlank()) {
                                    val filtered = patchGroups.filter { it.gameId.contains(gameId, ignoreCase = true) }
                                    if (filtered.isNotEmpty()) patchGroups = filtered
                                }
                                statusMessage = "✅ Downloaded ${patchGroups.sumOf { it.patches.size }} patches"
                            } else {
                                val errorMsg = result.exceptionOrNull()?.message ?: "Unknown error"
                                statusMessage = "❌ Error: $errorMsg"
                                Log.e("PatchManager", "Download failed: $errorMsg", result.exceptionOrNull())
                            }
                            isLoading = false
                        }
                    }
                ) {
                    Text("Download")
                }
            },
            dismissButton = {
                TextButton(onClick = { showDownloadDialog = false }) {
                    Text("Cancel")
                }
            }
        )
    }
}

@Composable
fun PatchGroupCard(
    group: PatchManager.PatchGroup,
    isExpanded: Boolean,
    onExpandToggle: () -> Unit,
    onPatchToggle: (PatchManager.Patch) -> Unit,
    isPatchEnabled: (PatchManager.Patch) -> Boolean
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(12.dp)
    ) {
        Column {
            // Header
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable(onClick = onExpandToggle)
                    .padding(16.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = group.gameName,
                        fontWeight = FontWeight.Bold,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                    Text(
                        text = "${group.gameId} • ${group.patches.size} patches",
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                Icon(
                    imageVector = if (isExpanded) Icons.Default.KeyboardArrowUp else Icons.Default.KeyboardArrowDown,
                    contentDescription = if (isExpanded) "Collapse" else "Expand"
                )
            }
            
            // Patches list
            if (isExpanded) {
                HorizontalDivider()
                group.patches.forEach { patch ->
                    PatchItem(
                        patch = patch,
                        isEnabled = isPatchEnabled(patch),
                        onToggle = { onPatchToggle(patch) }
                    )
                }
            }
        }
    }
}

@Composable
fun PatchItem(
    patch: PatchManager.Patch,
    isEnabled: Boolean,
    onToggle: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onToggle)
            .padding(horizontal = 16.dp, vertical = 12.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    text = patch.title,
                    fontWeight = FontWeight.Medium,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                if (patch.gameVersion.isNotBlank()) {
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(
                        text = patch.gameVersion,
                        fontSize = 10.sp,
                        color = Color.White,
                        modifier = Modifier
                            .background(
                                color = MaterialTheme.colorScheme.secondary,
                                shape = RoundedCornerShape(4.dp)
                            )
                            .padding(horizontal = 4.dp, vertical = 2.dp)
                    )
                }
            }
            if (patch.notes.isNotBlank()) {
                Text(
                    text = patch.notes,
                    fontSize = 12.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis
                )
            }
            Text(
                text = "by ${patch.author}",
                fontSize = 10.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
        Checkbox(
            checked = isEnabled,
            onCheckedChange = { onToggle() }
        )
    }
}
