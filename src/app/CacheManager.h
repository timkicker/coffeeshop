#pragma once

// Utility to clean up leftover temp files from crashed/interrupted downloads
class CacheManager {
public:
    // Delete all *.zip files in Paths::cacheDir()
    // Call once at app startup
    static void cleanupStaleZips();
};
