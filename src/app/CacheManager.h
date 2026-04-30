#pragma once

// Utility to clean up leftover temp files from crashed/interrupted downloads
class CacheManager {
public:
    // Delete all *.zip files in Paths::cacheDir()
    // Call once at app startup
    static void cleanupStaleZips();

    // Delete *.zip.partial files older than 24h. Recent partials are kept so
    // resume-after-restart can pick them up.
    static void cleanupStalePartials();

    // Delete any subdirectories under Paths::stagingDir() (leftover from a
    // crash mid-extract) and any "<modid>.old" siblings under sdcafiineBase()
    // (leftover from a crashed atomic-rename swap).
    static void cleanupStaleStaging();

    // Delete mod folders in sdcafiineBase() and disabledBase() that have no modinfo.json
    // These are left over from a crash during extraction
    static void cleanupCorruptMods();
};
