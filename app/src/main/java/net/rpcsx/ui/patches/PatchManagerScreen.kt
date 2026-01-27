package net.rpcsx.ui.patches

import android.content.Intent
import android.net.Uri
import android.util.Log
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.Divider
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.platform.LocalContext
import kotlinx.coroutines.launch
import net.rpcsx.GameRepository
import net.rpcsx.RPCSX
import net.rpcsx.utils.safeSettingsSet

data class PatchEntry(val id: String, val title: String, val description: String, val apply: (String?) -> Boolean)

@Composable
fun PatchManagerScreen(navigateBack: () -> Unit, gamePath: String?) {
    val games = remember { GameRepository.list() }
    var status by remember { mutableStateOf<String?>(null) }

    val matchedGame = games.find { it.info.path == gamePath }

    // Known patches registry (expandable)
    val registry = listOf(
        PatchEntry(id = "cod4_runtime", title = "Call of Duty 4 - Runtime Patch", description = "Apply runtime settings tuned for COD4") { path ->
            try {
                safeSettingsSet("Core@@PPU LLVM Version", "\"20.3\"")
                safeSettingsSet("Core@@SPU LLVM Version", "\"20.3\"")
                safeSettingsSet("Core@@PPU LLVM Greedy Mode", "true")
                safeSettingsSet("Core@@PPU Accurate Non-Java Mode", "false")
                safeSettingsSet("Core@@Use Accurate DFMA", "false")
                true
            } catch (e: Throwable) {
                Log.e("PatchManager", "apply cod4: ${e.message}")
                false
            }
        },
        PatchEntry(id = "no_op", title = "No-op Example Patch", description = "Example patch that does nothing", apply = { _ -> true })
    )

    Column(
        verticalArrangement = Arrangement.spacedBy(12.dp),
        modifier = Modifier.padding(16.dp)
    ) {
        Text(text = "Patch Manager")

        if (matchedGame != null) {
            Text(text = "Game: ${matchedGame.info.name.value ?: matchedGame.info.path}")

            val context = LocalContext.current
            val scope = rememberCoroutineScope()

            Button(onClick = {
                // Open RPCS3 wiki game patches page in browser
                val title = matchedGame.info.name.value ?: matchedGame.info.path
                val wikiUrl = "https://wiki.rpcs3.net/index.php?title=${java.net.URLEncoder.encode(title, "UTF-8")}" 
                try {
                    context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(wikiUrl)))
                } catch (e: Throwable) {
                    Log.e("PatchManager", "open wiki: ${e.message}")
                }
            }) { Text(text = "Open RPCS3 Wiki Page") }

            Button(onClick = {
                // Download wiki page HTML into app cache
                val title = matchedGame.info.name.value ?: matchedGame.info.path
                val wikiUrl = "https://wiki.rpcs3.net/index.php?title=${java.net.URLEncoder.encode(title, "UTF-8")}" 
                scope.launch {
                    try {
                        val html = java.net.URL(wikiUrl).readText()
                        val dir = java.io.File(context.filesDir, "patches")
                        if (!dir.exists()) dir.mkdirs()
                        val safe = title.replace(Regex("[^A-Za-z0-9._-]"), "_")
                        val out = java.io.File(dir, "${safe}.html")
                        out.writeText(html)
                        status = "Downloaded wiki page to ${out.absolutePath}"
                    } catch (e: Throwable) {
                        Log.e("PatchManager", "download wiki: ${e.message}")
                        status = "Failed to download wiki page: ${e.message}"
                    }
                }
            }) { Text(text = "Download RPCS3 Wiki Page") }

            // Parse saved wiki HTML (if present) and expose patch links
            var foundLinks by remember { mutableStateOf(listOf<Pair<String,String>>()) }

            val cacheDir = java.io.File(LocalContext.current.filesDir, "patches")
            val safeName = (matchedGame.info.name.value ?: matchedGame.info.path).replace(Regex("[^A-Za-z0-9._-]"), "_")
            val savedHtml = java.io.File(cacheDir, "${safeName}.html")
            if (savedHtml.exists() && foundLinks.isEmpty()) {
                try {
                    val html = savedHtml.readText()
                    foundLinks = parsePatchLinks(html, "https://wiki.rpcs3.net")
                } catch (e: Throwable) {
                    Log.e("PatchManager", "parse saved html: ${e.message}")
                }
            }

            if (foundLinks.isNotEmpty()) {
                LazyColumn(modifier = Modifier.fillMaxWidth()) {
                    items(foundLinks) { (href, text) ->
                        Column(modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
                            Text(text = text)
                            Text(text = href)
                            Button(onClick = {
                                scope.launch {
                                    try {
                                        val url = java.net.URL(href)
                                        val ext = href.substringAfterLast('.', "bin")
                                        if (!cacheDir.exists()) cacheDir.mkdirs()
                                        val out = java.io.File(cacheDir, "${safeName}_${System.currentTimeMillis()}.$ext")
                                        url.openStream().use { inp -> out.outputStream().use { it.write(inp.readBytes()) } }
                                        status = "Downloaded patch to ${out.absolutePath}"
                                    } catch (e: Throwable) {
                                        Log.e("PatchManager", "download link: ${e.message}")
                                        status = "Failed to download: ${e.message}"
                                    }
                                }
                            }) { Text(text = "Download") }

                            Button(onClick = {
                                scope.launch {
                                    try {
                                        val url = java.net.URL(href)
                                        val ext = href.substringAfterLast('.', "bin")
                                        if (!cacheDir.exists()) cacheDir.mkdirs()
                                        val out = java.io.File(cacheDir, "${safeName}_${System.currentTimeMillis()}.$ext")
                                        url.openStream().use { inp -> out.outputStream().use { it.write(inp.readBytes()) } }
                                        status = "Imported patch to ${out.absolutePath}"

                                        val nameLower = (matchedGame.info.name.value ?: "").lowercase()
                                        if (nameLower.contains("call of duty 4") || href.lowercase().contains("cod4") || text.lowercase().contains("cod4")) {
                                            safeSettingsSet("Core@@PPU LLVM Version", "\"20.3\"")
                                            safeSettingsSet("Core@@SPU LLVM Version", "\"20.3\"")
                                            safeSettingsSet("Core@@PPU LLVM Greedy Mode", "true")
                                            safeSettingsSet("Core@@PPU Accurate Non-Java Mode", "false")
                                            status = "Imported and applied COD4 runtime settings"
                                        }
                                    } catch (e: Throwable) {
                                        Log.e("PatchManager", "import link: ${e.message}")
                                        status = "Failed to import: ${e.message}"
                                    }
                                }
                            }) { Text(text = "Import & Apply") }

                            Divider()
                        }
                    }
                }
            } else {
                LazyColumn(modifier = Modifier.fillMaxWidth()) {
                    items(registry) { entry ->
                        Column(modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
                            Text(text = entry.title)
                            Text(text = entry.description)
                            Button(onClick = {
                                val ok = entry.apply(matchedGame.info.path)
                                status = if (ok) "Applied: ${entry.title}" else "Failed to apply: ${entry.title}"
                            }) {
                                Text(text = "Apply")
                            }
                            Divider()
                        }
                    }
                }
            }

            Button(onClick = { navigateBack() }) { Text(text = "Back") }
        } else {
            Text(text = "No game selected. Open Patch Manager from a game's long-press to auto-apply patches.")
            Button(onClick = { navigateBack() }) { Text(text = "Back") }
        }

        if (status != null) {
            Text(text = status!!)
        }
    }
}

// Very small HTML parser to extract anchors pointing to potential patches
fun parsePatchLinks(html: String, baseUrl: String): List<Pair<String,String>> {
    val results = ArrayList<Pair<String,String>>()
    val pattern = java.util.regex.Pattern.compile("<a[^>]+href=['\"]([^'\"]+)['\"][^>]*>(.*?)</a>", java.util.regex.Pattern.CASE_INSENSITIVE or java.util.regex.Pattern.DOTALL)
    val matcher = pattern.matcher(html)
    while (matcher.find()) {
        try {
            var href = matcher.group(1)
            val text = matcher.group(2).replace(Regex("<.*?>"), "").trim()
            if (!href.startsWith("http")) {
                href = if (href.startsWith("/")) baseUrl + href else baseUrl + "/" + href
            }
            val lower = href.lowercase() + " " + text.lowercase()
            if (lower.contains("patch") || lower.contains(".patch") || lower.contains(".diff") || lower.contains(".txt") || lower.contains("/patches") ) {
                results.add(Pair(href, text))
            }
        } catch (e: Throwable) {
            // ignore
        }
    }
    return results.distinctBy { it.first }
}
