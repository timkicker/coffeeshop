#pragma once

// Utility to clean up leftover temp files from crashed/interrupted downloads
class CacheManager {
public:
    // Delete all *.zip files in Paths::cacheDir()
    // Call once at app startup
    static void cleanupStaleZips();

    // Delete mod folders in sdcafiineBase() and disabledBase() that have no modinfo.json
    // These are left over from a crash during extraction
    static void cleanupCorruptMods();
};
