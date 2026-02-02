package net.rpcsx.ui.patches

import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.net.HttpURLConnection
import java.net.URL

/**
 * Patch Manager for RPCSX - ported from RPCS3
 * 
 * Handles downloading, parsing, and applying game patches from RPCS3 patch repository.
 * Patches are stored in YAML format and can be enabled/disabled per-game.
 */
object PatchManager {
    private const val TAG = "PatchManager"
    
    // RPCS3 patch repository URLs
    private const val RPCS3_PATCH_URL = "https://rpcs3.net/compatibility?api=v1&export=patches"
    private const val RPCS3_PATCH_RAW_URL = "https://raw.githubusercontent.com/RPCS3/rpcs3/master/bin/patches/patch.yml"
    private const val RPCS3_WIKI_PATCHES = "https://wiki.rpcs3.net/index.php?title=Help:Game_Patches&action=raw"
    
    // Local cache file name
    private const val PATCH_CACHE_FILE = "patches_cache.json"
    private const val PATCH_YML_FILE = "patch.yml"
    private const val ENABLED_PATCHES_FILE = "enabled_patches.json"
    
    data class Patch(
        val id: String,
        val gameId: String,
        val gameName: String,
        val title: String,
        val author: String,
        val notes: String,
        val patchVersion: String,
        val gameVersion: String,
        val enabled: Boolean = false,
        val patchData: String = "" // Raw patch content
    )
    
    data class PatchGroup(
        val gameId: String,
        val gameName: String,
        val patches: List<Patch>
    )
    
    /**
     * Check if the device has an active internet connection
     */
    fun isNetworkAvailable(context: Context): Boolean {
        try {
            val connectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as? ConnectivityManager
                ?: return false
            
            val network = connectivityManager.activeNetwork ?: return false
            val capabilities = connectivityManager.getNetworkCapabilities(network) ?: return false
            
            return capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET) &&
                   capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)
        } catch (e: Exception) {
            Log.e(TAG, "Error checking network: ${e.message}")
            return false
        }
    }
    
    /**
     * Get patches directory for storing patch files
     */
    fun getPatchesDir(context: Context): File {
        val dir = File(context.filesDir, "patches")
        if (!dir.exists()) {
            dir.mkdirs()
        }
        return dir
    }
    
    /**
     * Get enabled patches file
     */
    private fun getEnabledPatchesFile(context: Context): File {
        return File(getPatchesDir(context), ENABLED_PATCHES_FILE)
    }
    
    /**
     * Load enabled patches map from storage
     */
    fun loadEnabledPatches(context: Context): Map<String, Boolean> {
        return try {
            val file = getEnabledPatchesFile(context)
            if (file.exists()) {
                val json = JSONObject(file.readText())
                val map = mutableMapOf<String, Boolean>()
                json.keys().forEach { key ->
                    map[key] = json.optBoolean(key, false)
                }
                map
            } else {
                emptyMap()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load enabled patches: ${e.message}")
            emptyMap()
        }
    }
    
    /**
     * Save enabled patches map to storage
     */
    fun saveEnabledPatches(context: Context, enabledPatches: Map<String, Boolean>) {
        try {
            val file = getEnabledPatchesFile(context)
            val json = JSONObject()
            enabledPatches.forEach { (key, value) ->
                json.put(key, value)
            }
            file.writeText(json.toString(2))
            Log.i(TAG, "Saved ${enabledPatches.size} enabled patch settings")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to save enabled patches: ${e.message}")
        }
    }
    
    /**
     * Toggle patch enabled state
     */
    fun togglePatch(context: Context, patchId: String, enabled: Boolean) {
        val patches = loadEnabledPatches(context).toMutableMap()
        patches[patchId] = enabled
        saveEnabledPatches(context, patches)
        Log.i(TAG, "Patch $patchId ${if (enabled) "enabled" else "disabled"}")
    }
    
    /**
     * Check if a patch is enabled
     */
    fun isPatchEnabled(context: Context, patchId: String): Boolean {
        return loadEnabledPatches(context)[patchId] ?: false
    }
    
    /**
     * Download patches from RPCS3 repository
     */
    suspend fun downloadPatches(context: Context, onProgress: (Int, String) -> Unit = { _, _ -> }): Result<List<PatchGroup>> {
        return withContext(Dispatchers.IO) {
            try {
                // First check if network is available
                if (!isNetworkAvailable(context)) {
                    Log.e(TAG, "No network available")
                    return@withContext Result.failure(Exception("No internet connection. Please enable WiFi or mobile data and try again."))
                }
                
                onProgress(5, "Checking network connection...")
                Log.i(TAG, "Network available, starting download...")
                
                onProgress(10, "Connecting to RPCS3 patch server...")
                
                // Try to download from raw GitHub first (more reliable)
                val patchContent = try {
                    downloadUrl(RPCS3_PATCH_RAW_URL)
                } catch (e: Exception) {
                    Log.w(TAG, "Failed to download from GitHub: ${e.message}, trying wiki...")
                    onProgress(20, "Trying alternative source...")
                    try {
                        downloadUrl(RPCS3_WIKI_PATCHES)
                    } catch (e2: Exception) {
                        Log.e(TAG, "Failed to download patches: ${e2.message}")
                        return@withContext Result.failure(e2)
                    }
                }
                
                onProgress(50, "Parsing patches...")
                
                // Save raw patch file
                val patchFile = File(getPatchesDir(context), PATCH_YML_FILE)
                patchFile.writeText(patchContent)
                
                // Parse YAML patches
                val patches = parseYamlPatches(patchContent, context)
                
                onProgress(80, "Organizing patches...")
                
                // Group by game ID
                val groups = patches.groupBy { it.gameId }.map { (gameId, patchList) ->
                    PatchGroup(
                        gameId = gameId,
                        gameName = patchList.firstOrNull()?.gameName ?: gameId,
                        patches = patchList
                    )
                }
                
                // Cache the parsed result
                savePatchCache(context, groups)
                
                onProgress(100, "Done! Found ${patches.size} patches for ${groups.size} games")
                
                Result.success(groups)
            } catch (e: Exception) {
                Log.e(TAG, "Error downloading patches: ${e.message}")
                Result.failure(e)
            }
        }
    }
    
    /**
     * Load patches from cache
     */
    fun loadPatchesFromCache(context: Context): List<PatchGroup> {
        return try {
            val cacheFile = File(getPatchesDir(context), PATCH_CACHE_FILE)
            if (cacheFile.exists()) {
                val json = JSONObject(cacheFile.readText())
                val groups = mutableListOf<PatchGroup>()
                val groupsArray = json.optJSONArray("groups") ?: return emptyList()
                
                for (i in 0 until groupsArray.length()) {
                    val groupJson = groupsArray.getJSONObject(i)
                    val patches = mutableListOf<Patch>()
                    val patchesArray = groupJson.optJSONArray("patches") ?: JSONArray()
                    
                    for (j in 0 until patchesArray.length()) {
                        val patchJson = patchesArray.getJSONObject(j)
                        patches.add(Patch(
                            id = patchJson.optString("id", ""),
                            gameId = patchJson.optString("gameId", ""),
                            gameName = patchJson.optString("gameName", ""),
                            title = patchJson.optString("title", ""),
                            author = patchJson.optString("author", ""),
                            notes = patchJson.optString("notes", ""),
                            patchVersion = patchJson.optString("patchVersion", ""),
                            gameVersion = patchJson.optString("gameVersion", ""),
                            enabled = isPatchEnabled(context, patchJson.optString("id", "")),
                            patchData = patchJson.optString("patchData", "")
                        ))
                    }
                    
                    groups.add(PatchGroup(
                        gameId = groupJson.optString("gameId", ""),
                        gameName = groupJson.optString("gameName", ""),
                        patches = patches
                    ))
                }
                
                groups
            } else {
                emptyList()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load patch cache: ${e.message}")
            emptyList()
        }
    }
    
    /**
     * Save patches to cache
     */
    private fun savePatchCache(context: Context, groups: List<PatchGroup>) {
        try {
            val cacheFile = File(getPatchesDir(context), PATCH_CACHE_FILE)
            val json = JSONObject()
            val groupsArray = JSONArray()
            
            for (group in groups) {
                val groupJson = JSONObject()
                groupJson.put("gameId", group.gameId)
                groupJson.put("gameName", group.gameName)
                
                val patchesArray = JSONArray()
                for (patch in group.patches) {
                    val patchJson = JSONObject()
                    patchJson.put("id", patch.id)
                    patchJson.put("gameId", patch.gameId)
                    patchJson.put("gameName", patch.gameName)
                    patchJson.put("title", patch.title)
                    patchJson.put("author", patch.author)
                    patchJson.put("notes", patch.notes)
                    patchJson.put("patchVersion", patch.patchVersion)
                    patchJson.put("gameVersion", patch.gameVersion)
                    patchJson.put("patchData", patch.patchData)
                    patchesArray.put(patchJson)
                }
                groupJson.put("patches", patchesArray)
                groupsArray.put(groupJson)
            }
            
            json.put("groups", groupsArray)
            json.put("timestamp", System.currentTimeMillis())
            
            cacheFile.writeText(json.toString(2))
            Log.i(TAG, "Saved patch cache with ${groups.size} groups")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to save patch cache: ${e.message}")
        }
    }
    
    /**
     * Parse YAML patch file (simplified parser for RPCS3 patch format)
     */
    private fun parseYamlPatches(content: String, context: Context): List<Patch> {
        val patches = mutableListOf<Patch>()
        val enabledPatches = loadEnabledPatches(context)
        
        try {
            // RPCS3 patch.yml format:
            // Version: 1.2
            // 
            // GAME_ID:
            //   "Game Name":
            //     Patch Name:
            //       Games:
            //         "Game Name":
            //           GAME_ID: [ All ]
            //       Author: Author Name
            //       Notes: Some notes
            //       Patch Version: 1.0
            //       Patch:
            //         - [ be32, 0x12345678, 0x00000000 ]
            
            val lines = content.lines()
            var currentGameId = ""
            var currentGameName = ""
            var currentPatchTitle = ""
            var currentAuthor = ""
            var currentNotes = ""
            var currentPatchVersion = ""
            var currentGameVersion = "All"
            var inPatch = false
            val patchDataBuilder = StringBuilder()
            
            for (line in lines) {
                val trimmed = line.trim()
                
                // Skip empty lines and comments
                if (trimmed.isEmpty() || trimmed.startsWith("#")) continue
                
                // Detect game ID (starts at column 0, ends with :)
                if (!line.startsWith(" ") && !line.startsWith("\t") && 
                    trimmed.matches(Regex("^[A-Z]{4}[0-9]{5}:.*"))) {
                    
                    // Save previous patch if exists
                    if (currentPatchTitle.isNotEmpty() && currentGameId.isNotEmpty()) {
                        val patchId = "${currentGameId}_${currentPatchTitle}".replace(" ", "_")
                        patches.add(Patch(
                            id = patchId,
                            gameId = currentGameId,
                            gameName = currentGameName,
                            title = currentPatchTitle,
                            author = currentAuthor,
                            notes = currentNotes,
                            patchVersion = currentPatchVersion,
                            gameVersion = currentGameVersion,
                            enabled = enabledPatches[patchId] ?: false,
                            patchData = patchDataBuilder.toString()
                        ))
                        patchDataBuilder.clear()
                    }
                    
                    currentGameId = trimmed.substringBefore(":")
                    currentPatchTitle = ""
                    currentAuthor = ""
                    currentNotes = ""
                    currentPatchVersion = ""
                    inPatch = false
                    continue
                }
                
                // Detect game name (indented, in quotes, ends with :)
                if (line.startsWith("  ") && !line.startsWith("    ") && 
                    trimmed.startsWith("\"") && trimmed.endsWith(":")) {
                    currentGameName = trimmed.removeSurrounding("\"", "\":")
                    continue
                }
                
                // Detect patch title (4 spaces indent)
                if (line.startsWith("    ") && !line.startsWith("      ") &&
                    !trimmed.startsWith("-") && trimmed.endsWith(":") &&
                    !trimmed.startsWith("Games:") && !trimmed.startsWith("Author:") &&
                    !trimmed.startsWith("Notes:") && !trimmed.startsWith("Patch:") &&
                    !trimmed.startsWith("Patch Version:") && !trimmed.startsWith("Group:")) {
                    
                    // Save previous patch if exists
                    if (currentPatchTitle.isNotEmpty() && currentGameId.isNotEmpty()) {
                        val patchId = "${currentGameId}_${currentPatchTitle}".replace(" ", "_")
                        patches.add(Patch(
                            id = patchId,
                            gameId = currentGameId,
                            gameName = currentGameName,
                            title = currentPatchTitle,
                            author = currentAuthor,
                            notes = currentNotes,
                            patchVersion = currentPatchVersion,
                            gameVersion = currentGameVersion,
                            enabled = enabledPatches[patchId] ?: false,
                            patchData = patchDataBuilder.toString()
                        ))
                        patchDataBuilder.clear()
                    }
                    
                    currentPatchTitle = trimmed.removeSuffix(":")
                    currentAuthor = ""
                    currentNotes = ""
                    currentPatchVersion = ""
                    inPatch = false
                    continue
                }
                
                // Parse patch metadata
                when {
                    trimmed.startsWith("Author:") -> {
                        currentAuthor = trimmed.removePrefix("Author:").trim()
                    }
                    trimmed.startsWith("Notes:") -> {
                        currentNotes = trimmed.removePrefix("Notes:").trim()
                    }
                    trimmed.startsWith("Patch Version:") -> {
                        currentPatchVersion = trimmed.removePrefix("Patch Version:").trim()
                    }
                    trimmed.startsWith("Patch:") -> {
                        inPatch = true
                    }
                    inPatch && trimmed.startsWith("-") -> {
                        patchDataBuilder.appendLine(line)
                    }
                }
            }
            
            // Don't forget last patch
            if (currentPatchTitle.isNotEmpty() && currentGameId.isNotEmpty()) {
                val patchId = "${currentGameId}_${currentPatchTitle}".replace(" ", "_")
                patches.add(Patch(
                    id = patchId,
                    gameId = currentGameId,
                    gameName = currentGameName,
                    title = currentPatchTitle,
                    author = currentAuthor,
                    notes = currentNotes,
                    patchVersion = currentPatchVersion,
                    gameVersion = currentGameVersion,
                    enabled = enabledPatches[patchId] ?: false,
                    patchData = patchDataBuilder.toString()
                ))
            }
            
        } catch (e: Exception) {
            Log.e(TAG, "Error parsing patches: ${e.message}")
        }
        
        Log.i(TAG, "Parsed ${patches.size} patches")
        return patches
    }
    
    /**
     * Get patches for a specific game
     */
    fun getPatchesForGame(context: Context, gameId: String): List<Patch> {
        val allPatches = loadPatchesFromCache(context)
        return allPatches.find { it.gameId == gameId }?.patches ?: emptyList()
    }
    
    /**
     * Search patches by game name or ID
     */
    fun searchPatches(context: Context, query: String): List<PatchGroup> {
        val allPatches = loadPatchesFromCache(context)
        val lowerQuery = query.lowercase()
        
        return allPatches.filter { group ->
            group.gameId.lowercase().contains(lowerQuery) ||
            group.gameName.lowercase().contains(lowerQuery) ||
            group.patches.any { it.title.lowercase().contains(lowerQuery) }
        }
    }
    
    /**
     * Get enabled patches for a game (ready to apply)
     */
    fun getEnabledPatchesForGame(context: Context, gameId: String): List<Patch> {
        return getPatchesForGame(context, gameId).filter { it.enabled }
    }
    
    /**
     * Apply patches to game memory via native RPCSX code
     */
    fun applyPatches(context: Context, gameId: String): Boolean {
        val enabledPatches = getEnabledPatchesForGame(context, gameId)
        
        if (enabledPatches.isEmpty()) {
            Log.i(TAG, "No enabled patches for game $gameId")
            return true
        }
        
        Log.i(TAG, "Applying ${enabledPatches.size} patches to game $gameId")
        
        // Build comma-separated list of enabled patch IDs
        val enabledPatchIds = enabledPatches.joinToString(",") { it.id }
        
        // Get patch file path
        val patchFile = File(getPatchesDir(context), PATCH_YML_FILE)
        val patchFilePath = if (patchFile.exists()) patchFile.absolutePath else ""
        
        // Call native patch application
        return try {
            val appliedCount = net.rpcsx.RPCSX.instance.applyGamePatches(
                patchFilePath,
                gameId,
                enabledPatchIds
            )
            Log.i(TAG, "Native patches applied: $appliedCount")
            
            for (patch in enabledPatches) {
                Log.i(TAG, "  ✓ ${patch.title} by ${patch.author}")
            }
            
            appliedCount > 0
        } catch (e: Exception) {
            Log.e(TAG, "Failed to apply patches via native code: ${e.message}")
            false
        }
    }
    
    /**
     * Check if game has patches available (built-in or downloaded)
     */
    fun hasPatches(gameId: String): Boolean {
        return try {
            net.rpcsx.RPCSX.instance.hasGamePatches(gameId)
        } catch (e: Exception) {
            Log.w(TAG, "Failed to check patches: ${e.message}")
            false
        }
    }
    
    /**
     * Download content from URL with better error handling
     */
    private fun downloadUrl(urlString: String): String {
        Log.i(TAG, "Downloading from: $urlString")
        
        try {
            val url = URL(urlString)
            val connection = url.openConnection() as HttpURLConnection
            connection.requestMethod = "GET"
            connection.connectTimeout = 30000
            connection.readTimeout = 60000 // Longer read timeout
            // Use browser-like User-Agent to avoid 403 Forbidden
            connection.setRequestProperty("User-Agent", "Mozilla/5.0 (Linux; Android 14) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Mobile Safari/537.36")
            connection.setRequestProperty("Accept", "text/plain, text/yaml, text/html, application/xhtml+xml, */*")
            connection.setRequestProperty("Accept-Language", "en-US,en;q=0.9")
            connection.setRequestProperty("Accept-Encoding", "identity") // No compression to simplify
            connection.setRequestProperty("Connection", "keep-alive")
            connection.instanceFollowRedirects = true
            
            try {
                connection.connect()
                val responseCode = connection.responseCode
                Log.i(TAG, "Response code: $responseCode")
                
                if (responseCode == HttpURLConnection.HTTP_OK) {
                    val content = connection.inputStream.bufferedReader().use { it.readText() }
                    Log.i(TAG, "Downloaded ${content.length} bytes")
                    return content
                } else if (responseCode == HttpURLConnection.HTTP_MOVED_TEMP || 
                           responseCode == HttpURLConnection.HTTP_MOVED_PERM ||
                           responseCode == 307 || responseCode == 308) {
                    // Handle redirects manually
                    val newUrl = connection.getHeaderField("Location")
                    Log.i(TAG, "Redirecting to: $newUrl")
                    connection.disconnect()
                    return downloadUrl(newUrl)
                } else if (responseCode == HttpURLConnection.HTTP_FORBIDDEN) {
                    Log.e(TAG, "HTTP 403 Forbidden - server rejected request")
                    throw Exception("Access denied (403). The server blocked the request. Try again later.")
                } else if (responseCode == 429) {
                    Log.e(TAG, "HTTP 429 Too Many Requests - rate limited")
                    throw Exception("Too many requests. Please wait a few minutes and try again.")
                } else {
                    val errorMsg = connection.errorStream?.bufferedReader()?.use { it.readText() } ?: "Server error"
                    Log.e(TAG, "HTTP $responseCode: $errorMsg")
                    throw Exception("Server returned error $responseCode. Try again later.")
                }
            } finally {
                connection.disconnect()
            }
        } catch (e: java.net.UnknownHostException) {
            Log.e(TAG, "DNS error - no internet or host unreachable: ${e.message}")
            throw Exception("No internet connection. Please check your network settings.")
        } catch (e: java.net.SocketTimeoutException) {
            Log.e(TAG, "Connection timeout: ${e.message}")
            throw Exception("Connection timeout. Server may be slow or unreachable.")
        } catch (e: javax.net.ssl.SSLException) {
            Log.e(TAG, "SSL error: ${e.message}")
            throw Exception("SSL/TLS error. Try again later or check your network.")
        } catch (e: java.io.IOException) {
            Log.e(TAG, "IO error: ${e.message}")
            throw Exception("Network error: ${e.message}")
        }
    }
    
    /**
     * Extract game ID from game path (e.g., /path/to/BLUS12345/... -> BLUS12345)
     */
    fun extractGameId(gamePath: String): String? {
        val regex = Regex("[A-Z]{4}[0-9]{5}")
        return regex.find(gamePath)?.value
    }
}
