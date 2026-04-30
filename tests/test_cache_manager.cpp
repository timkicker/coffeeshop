#include <catch2/catch_test_macros.hpp>
#include "app/CacheManager.h"
#include "app/Paths.h"

#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <cstdio>
#include <dirent.h>

namespace {

static std::string testRoot() {
    return std::string("/tmp/coffeeshop_cache_") + std::to_string(getpid());
}

static void rmTree(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return;
    if (!S_ISDIR(st.st_mode)) { remove(path.c_str()); return; }
    DIR* d = opendir(path.c_str());
    if (d) {
        struct dirent* e;
        while ((e = readdir(d)) != nullptr) {
            std::string n = e->d_name;
            if (n == "." || n == "..") continue;
            rmTree(path + "/" + n);
        }
        closedir(d);
    }
    rmdir(path.c_str());
}

static void mkdirs(const std::string& path) {
    for (size_t i = 1; i <= path.size(); i++)
        if (i == path.size() || path[i] == '/')
            mkdir(path.substr(0, i).c_str(), 0755);
}

static void touch(const std::string& path) {
    FILE* f = fopen(path.c_str(), "wb");
    if (f) fclose(f);
}

static bool exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

struct Fixture {
    std::string root;
    Fixture() {
        root = testRoot();
        rmTree(root);
        mkdirs(root);
        Paths::testRootOverride = root;
    }
    ~Fixture() {
        Paths::testRootOverride.clear();
        rmTree(root);
    }
};

} // namespace

// ─── cleanupStaleZips ──────────────────────────────────────────────────────

TEST_CASE("CacheManager::cleanupStaleZips - missing cache dir is no-op", "[cache]") {
    Fixture fx;
    // Don't create cache dir
    CacheManager::cleanupStaleZips(); // should not crash
}

TEST_CASE("CacheManager::cleanupStaleZips - removes .zip files", "[cache]") {
    Fixture fx;
    auto cacheDir = Paths::cacheDir();
    mkdirs(cacheDir);
    touch(cacheDir + "/mod1.zip");
    touch(cacheDir + "/mod2.zip");

    CacheManager::cleanupStaleZips();

    REQUIRE_FALSE(exists(cacheDir + "/mod1.zip"));
    REQUIRE_FALSE(exists(cacheDir + "/mod2.zip"));
}

TEST_CASE("CacheManager::cleanupStaleZips - leaves non-zip files alone", "[cache]") {
    Fixture fx;
    auto cacheDir = Paths::cacheDir();
    mkdirs(cacheDir);
    touch(cacheDir + "/keep.txt");
    touch(cacheDir + "/something.json");
    touch(cacheDir + "/delete-me.zip");

    CacheManager::cleanupStaleZips();

    REQUIRE(exists(cacheDir + "/keep.txt"));
    REQUIRE(exists(cacheDir + "/something.json"));
    REQUIRE_FALSE(exists(cacheDir + "/delete-me.zip"));
}

TEST_CASE("CacheManager::cleanupStaleZips - file shorter than 4 chars is skipped", "[cache]") {
    Fixture fx;
    auto cacheDir = Paths::cacheDir();
    mkdirs(cacheDir);
    touch(cacheDir + "/a");
    touch(cacheDir + "/x.zip");

    CacheManager::cleanupStaleZips();

    REQUIRE(exists(cacheDir + "/a"));        // too short, untouched
    REQUIRE_FALSE(exists(cacheDir + "/x.zip"));
}

// ─── cleanupCorruptMods ────────────────────────────────────────────────────

TEST_CASE("CacheManager::cleanupCorruptMods - missing dirs are no-op", "[cache]") {
    Fixture fx;
    CacheManager::cleanupCorruptMods(); // should not crash
}

TEST_CASE("CacheManager::cleanupCorruptMods - removes mod folder without modinfo.json", "[cache]") {
    Fixture fx;
    std::string corrupt = Paths::sdcafiineBase() + "/0005000010101000/broken-mod";
    mkdirs(corrupt);
    touch(corrupt + "/something.szs");

    CacheManager::cleanupCorruptMods();

    REQUIRE_FALSE(exists(corrupt));
}

TEST_CASE("CacheManager::cleanupCorruptMods - keeps mod with modinfo.json", "[cache]") {
    Fixture fx;
    std::string ok = Paths::sdcafiineBase() + "/0005000010101000/good-mod";
    mkdirs(ok);
    std::ofstream f(ok + "/modinfo.json");
    f << R"({"id":"good-mod","version":"1.0"})";
    f.close();

    CacheManager::cleanupCorruptMods();

    REQUIRE(exists(ok));
    REQUIRE(exists(ok + "/modinfo.json"));
}

TEST_CASE("CacheManager::cleanupCorruptMods - mixed corrupt and intact", "[cache]") {
    Fixture fx;
    std::string base = Paths::sdcafiineBase() + "/0005000010101000";
    std::string ok      = base + "/intact";
    std::string corrupt = base + "/corrupt";
    mkdirs(ok);
    mkdirs(corrupt);
    std::ofstream f(ok + "/modinfo.json");
    f << "{}";
    f.close();
    touch(corrupt + "/file.szs");

    CacheManager::cleanupCorruptMods();

    REQUIRE(exists(ok));
    REQUIRE_FALSE(exists(corrupt));
}

TEST_CASE("CacheManager::cleanupCorruptMods - also cleans disabled folder", "[cache]") {
    Fixture fx;
    std::string corrupt = Paths::disabledBase() + "/0005000010101000/broken-mod";
    mkdirs(corrupt);
    touch(corrupt + "/file.szs");

    CacheManager::cleanupCorruptMods();

    REQUIRE_FALSE(exists(corrupt));
}
