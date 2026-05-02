#include <catch2/catch_test_macros.hpp>
#include "net/RepoManager.h"
#include "mods/InstalledScanner.h"
#include "mods/InstallChecker.h"
#include "mods/ConflictChecker.h"
#include "mods/ZipExtractor.h"
#include "app/Paths.h"
#include "app/Config.h"
#include "curl_mock.h"

#include <zlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <cstdio>
#include <dirent.h>
#include <cstdint>

namespace {

static std::string testRoot() {
    return std::string("/tmp/coffeeshop_integration_") + std::to_string(getpid());
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

static bool exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

static std::string readFile(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string out(n, '\0');
    if (n > 0) fread(&out[0], 1, n, f);
    fclose(f);
    return out;
}

// Build a tiny valid stored-method ZIP with a single file
static std::vector<uint8_t> buildSimpleZip(const std::string& name,
                                            const std::string& content) {
    std::vector<uint8_t> v;
    auto u16 = [&](uint16_t x) { v.push_back(x & 0xff); v.push_back((x >> 8) & 0xff); };
    auto u32 = [&](uint32_t x) {
        v.push_back(x & 0xff); v.push_back((x >> 8) & 0xff);
        v.push_back((x >> 16) & 0xff); v.push_back((x >> 24) & 0xff);
    };
    auto bytes = [&](const void* d, size_t n) {
        auto* p = (const uint8_t*)d;
        v.insert(v.end(), p, p + n);
    };

    u32(0x04034b50);          // local file sig
    u16(20);                  // version
    u16(0);                   // flags
    u16(0);                   // method = stored
    u16(0); u16(0); u32(0);   // time/date/crc
    u32(content.size());      // compSize
    u32(content.size());      // uncompSize
    u16(name.size());         // nameLen
    u16(0);                   // extraLen
    bytes(name.data(), name.size());
    bytes(content.data(), content.size());

    // EOCD
    u32(0x06054b50);
    for (int i = 0; i < 16; i++) v.push_back(0);
    u16(0);
    return v;
}

struct Fixture {
    std::string root;
    Fixture() {
        root = testRoot();
        rmTree(root);
        mkdirs(root);
        Paths::testRootOverride = root;
        InstalledScanner::invalidate();
        CurlMock::reset();
    }
    ~Fixture() {
        Paths::testRootOverride.clear();
        CurlMock::reset();
        rmTree(root);
        InstalledScanner::invalidate();
    }
};

} // namespace

// ─── Repo fetch -> InstalledScanner integration ────────────────────────────

TEST_CASE("integration: fetched repo + manually installed mod -> scanner sees match",
          "[integration]") {
    Fixture fx;

    // 1. Repo exposes a mod
    CurlMock::setResponse("https://repo.example.com/repo.json", R"({
        "name": "Test Repo",
        "games": [{
            "name": "Mario Kart 8",
            "titleIds": ["000500001010EB00"],
            "mods": [{
                "id": "fancy-skin",
                "name": "Fancy Skin",
                "version": "2.0.0",
                "author": "X",
                "download": "https://example.com/fancy.zip"
            }]
        }]
    })");

    RepoManager rm;
    rm.fetch("https://repo.example.com/repo.json");
    REQUIRE(rm.repo().games.size() == 1);
    REQUIRE(rm.repo().games[0].mods.size() == 1);

    // 2. Simulate that the user installed it (skipping the actual download)
    std::string modDir = Paths::sdcafiineBase() + "/000500001010EB00/fancy-skin";
    mkdirs(modDir);
    std::ofstream(modDir + "/modinfo.json")
        << R"({"id":"fancy-skin","version":"1.5.0"})";

    // 3. InstallChecker should detect it and flag an update
    auto status = InstallChecker::check("fancy-skin", "2.0.0",
                                         {"000500001010EB00"});
    REQUIRE(status.installed);
    REQUIRE(status.updateAvail); // installed=1.5.0 < repo=2.0.0
    REQUIRE(status.installedVersion == "1.5.0");

    // 4. InstalledScanner sees it as active
    auto scanned = InstalledScanner::scan();
    REQUIRE(scanned.size() == 1);
    REQUIRE(scanned[0].id == "fancy-skin");
    REQUIRE(scanned[0].version == "1.5.0");
    REQUIRE(scanned[0].active);
}

TEST_CASE("integration: ZipExtractor + InstalledScanner end-to-end", "[integration]") {
    Fixture fx;

    // Build a fake mod ZIP and "install" it via ZipExtractor
    auto zipBytes = buildSimpleZip("modinfo.json",
                                    R"({"id":"my-mod","version":"3.0.0"})");
    std::string zipPath = fx.root + "/mod.zip";
    {
        FILE* f = fopen(zipPath.c_str(), "wb");
        fwrite(zipBytes.data(), 1, zipBytes.size(), f);
        fclose(f);
    }

    std::string destDir = Paths::sdcafiineBase() + "/0005000010101000/my-mod";
    mkdirs(destDir);
    REQUIRE(ZipExtractor::extract(zipPath, destDir));

    // Now scanner should pick it up
    auto mods = InstalledScanner::scan();
    REQUIRE(mods.size() == 1);
    REQUIRE(mods[0].id == "my-mod");
    REQUIRE(mods[0].version == "3.0.0");
}

// ─── activate/deactivate roundtrip with conflict detection ─────────────────

TEST_CASE("integration: deactivate -> reactivate workflow", "[integration]") {
    Fixture fx;

    // Install mod
    std::string dir = Paths::sdcafiineBase() + "/0005000010101000/my-mod";
    mkdirs(dir);
    std::ofstream(dir + "/modinfo.json") << R"({"id":"my-mod","version":"1.0"})";

    auto mods = InstalledScanner::scan();
    REQUIRE(mods.size() == 1);
    REQUIRE(mods[0].active);

    // Deactivate
    InstalledMod mod = mods[0];
    REQUIRE(InstalledScanner::setActive(mod, false));

    // After deactivation, scan should still find it (now in disabled/)
    auto mods2 = InstalledScanner::scan();
    REQUIRE(mods2.size() == 1);
    REQUIRE_FALSE(mods2[0].active);

    // Reactivate
    InstalledMod mod2 = mods2[0];
    REQUIRE(InstalledScanner::setActive(mod2, true));

    auto mods3 = InstalledScanner::scan();
    REQUIRE(mods3.size() == 1);
    REQUIRE(mods3[0].active);
    REQUIRE(exists(dir + "/modinfo.json"));
}

TEST_CASE("integration: conflict-checker against scanned active mods", "[integration]") {
    Fixture fx;

    // Active mod that owns content/foo.szs (we just record the path symbolically)
    std::vector<std::string> newMod    = {"content/driver/Mario.szs",
                                           "content/driver/Wario.szs"};
    std::vector<ConflictChecker::ModFiles> active = {
        {"existing-mod", {"content/driver/Wario.szs"}}
    };

    auto result = ConflictChecker::checkFiles(newMod, active);
    REQUIRE(result.hasConflict);
    REQUIRE(result.conflictingMods.size() == 1);
    REQUIRE(result.conflictingMods[0] == "existing-mod");
}

// ─── Config-driven repo fetch ──────────────────────────────────────────────

TEST_CASE("integration: Config repos drive RepoManager fetches", "[integration]") {
    Fixture fx;

    // Config saved by user
    std::string cfgPath = fx.root + "/config.json";
    Config c;
    c.repos = {"https://r1.example.com/repo.json",
               "https://r2.example.com/repo.json"};
    REQUIRE(c.saveTo(cfgPath));

    // Load it back and use repos as if from a real config
    Config c2;
    REQUIRE(c2.loadFrom(cfgPath));
    REQUIRE(c2.repos.size() == 2);

    CurlMock::setResponse(c2.repos[0], R"({"name":"R1","games":[]})");
    CurlMock::setResponse(c2.repos[1], R"({"name":"R2","games":[]})");

    RepoManager rm;
    rm.fetch(c2.repos[0]);
    REQUIRE(rm.repo().name == "R1");
    rm.fetch(c2.repos[1]);
    REQUIRE(rm.repo().name == "R2");
    REQUIRE(CurlMock::performCount() == 2);
}
