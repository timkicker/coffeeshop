#pragma once
#include <string>
#include <vector>

struct InstalledMod {
    std::string id;
    std::string titleId;
    std::string name;        // from modinfo.json, falls back to id
    std::string version;     // from modinfo.json
    std::string repoVersion; // from repo (empty if not matched)
    bool        active;      // true = in sdcafiine/, false = in disabled/
    std::string path;        // full path to mod folder
    std::string installedAt; // from modinfo.json (e.g. "Apr 21 2026"); empty if unknown
};

class InstalledScanner {
public:
    // Returns a *snapshot copy* of the cached installed-mods list. Cache is
    // populated on first call and after invalidate(). Returns by value (not
    // reference) because the cache is shared across threads -- a held
    // reference would race with concurrent invalidate() from the download
    // worker thread. Boot used to call scan() 3-4 times racing with the
    // fetch thread; caching cuts SD I/O by 70%+.
    static std::vector<InstalledMod> scan();

    // Force re-scan from disk on the next scan() call. Thread-safe.
    // Call after any operation that changes the on-disk state.
    static void invalidate();

    // Returns a counter that increments on every invalidate(). Lets observers
    // (e.g. MainLayout) detect that some other screen mutated the install
    // state, so they can refresh their own cached views without polling
    // scan() every frame. Thread-safe.
    static unsigned generation();

    // Move mod between sdcafiine/ and disabled/. Invalidates cache on success.
    static bool setActive(InstalledMod& mod, bool active);

    // Delete mod folder entirely (from wherever it currently lives).
    // Invalidates cache on success.
    static bool remove(const InstalledMod& mod);

    static bool hasUpdate(const InstalledMod& mod);

private:
    static std::vector<InstalledMod> doScan();
};
