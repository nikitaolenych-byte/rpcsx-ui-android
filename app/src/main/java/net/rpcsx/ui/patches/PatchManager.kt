package net.rpcsx.ui.patches

import android.content.Context
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
                onProgress(10, "Connecting to RPCS3 patch server...")
                
                // Try to download from raw GitHub first (more reliable)
                val patchContent = try {
                    downloadUrl(RPCS3_PATCH_RAW_URL)
                } catch (e: Exception) {
                    Log.w(TAG, "Failed to download from GitHub, trying wiki...")
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
     * Apply patches to game memory (placeholder - actual implementation depends on native code)
     */
    fun applyPatches(context: Context, gameId: String): Boolean {
        val enabledPatches = getEnabledPatchesForGame(context, gameId)
        
        if (enabledPatches.isEmpty()) {
            Log.i(TAG, "No enabled patches for game $gameId")
            return true
        }
        
        Log.i(TAG, "Applying ${enabledPatches.size} patches to game $gameId")
        
        // TODO: Implement actual patch application via native code
        // This would call into the RPCSX native library to modify game memory
        // based on the patch data (be32, be64, etc. commands)
        
        for (patch in enabledPatches) {
            Log.i(TAG, "  - ${patch.title} by ${patch.author}")
        }
        
        return true
    }
    
    /**
     * Download content from URL
     */
    private fun downloadUrl(urlString: String): String {
        val url = URL(urlString)
        val connection = url.openConnection() as HttpURLConnection
        connection.requestMethod = "GET"
        connection.connectTimeout = 30000
        connection.readTimeout = 30000
        connection.setRequestProperty("User-Agent", "RPCSX-Android/1.0")
        
        try {
            val responseCode = connection.responseCode
            if (responseCode == HttpURLConnection.HTTP_OK) {
                return connection.inputStream.bufferedReader().use { it.readText() }
            } else {
                throw Exception("HTTP error: $responseCode")
            }
        } finally {
            connection.disconnect()
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
