// Lightweight NBTC prototype header
#pragma once

#include <string>

namespace nbtc {

// Initialize engine (load model if available). Returns true on success.
bool Initialize(const std::string& model_path = "");

// Analyze a game's binary data (path to unpacked game content) and generate cached optimized blocks.
// This is a best-effort prototype that stores metadata to disk under app-specific cache.
bool AnalyzeAndCache(const std::string& game_path);

// Try to load cached optimized binary for given game id/path. Returns true if cache loaded.
bool LoadCacheForGame(const std::string& game_id);

// Shutdown and flush state
void Shutdown();

} // namespace nbtc
