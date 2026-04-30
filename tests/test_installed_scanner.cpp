#include <catch2/catch_test_macros.hpp>
#include "mods/InstalledScanner.h"
#include "app/Paths.h"

#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <cstdio>
#include <dirent.h>
#include <algorithm>

namespace {

static std::string testRoot() {
    return std::string("/tmp/coffeeshop_scan_") + std::to_string(getpid());
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

static void writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    f << content;
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

    void installActive(const std::string& titleId, const std::string& modId,
                       const std::string& version) {
        std::string dir = Paths::sdcafiineBase() + "/" + titleId + "/" + modId;
        mkdirs(dir);
        if (!version.empty())
            writeFile(dir + "/modinfo.json",
                      "{\"id\":\"" + modId + "\",\"version\":\"" + version + "\"}");
    }

    void installDisabled(const std::string& titleId, const std::string& modId,
                         const std::string& version) {
        std::string dir = Paths::disabledBase() + "/" + titleId + "/" + modId;
        mkdirs(dir);
        if (!version.empty())
            writeFile(dir + "/modinfo.json",
                      "{\"id\":\"" + modId + "\",\"version\":\"" + version + "\"}");
    }
};

static const InstalledMod* findMod(const std::vector<InstalledMod>& list,
                                    const std::string& id) {
    for (auto& m : list) if (m.id == id) return &m;
    return nullptr;
}

} // namespace

// ─── hasUpdate (already covered, keep for regression) ────────────────────

TEST_CASE("InstalledScanner::hasUpdate - same version", "[update]") {
    InstalledMod mod;
    mod.version     = "1.0.0";
    mod.repoVersion = "1.0.0";
    REQUIRE_FALSE(InstalledScanner::hasUpdate(mod));
}

TEST_CASE("InstalledScanner::hasUpdate - newer repo version", "[update]") {
    InstalledMod mod;
    mod.version     = "1.0.0";
    mod.repoVersion = "1.1.0";
    REQUIRE(InstalledScanner::hasUpdate(mod));
}

TEST_CASE("InstalledScanner::hasUpdate - empty repoVersion means no update info", "[update]") {
    InstalledMod mod;
    mod.version     = "1.0.0";
    mod.repoVersion = "";
    REQUIRE_FALSE(InstalledScanner::hasUpdate(mod));
}

TEST_CASE("InstalledScanner::hasUpdate - older repo version still triggers update flag", "[update]") {
    InstalledMod mod;
    mod.version     = "1.1.0";
    mod.repoVersion = "1.0.0";
    REQUIRE(InstalledScanner::hasUpdate(mod));
}

// ─── scan() ────────────────────────────────────────────────────────────────

TEST_CASE("InstalledScanner::scan - empty filesystem returns empty list", "[scan]") {
    Fixture fx;
    auto mods = InstalledScanner::scan();
    REQUIRE(mods.empty());
}

TEST_CASE("InstalledScanner::scan - finds active mod", "[scan]") {
    Fixture fx;
    fx.installActive("0005000010101000", "my-mod", "1.0.0");

    auto mods = InstalledScanner::scan();
    REQUIRE(mods.size() == 1);
    REQUIRE(mods[0].id == "my-mod");
    REQUIRE(mods[0].titleId == "0005000010101000");
    REQUIRE(mods[0].version == "1.0.0");
    REQUIRE(mods[0].active);
}

TEST_CASE("InstalledScanner::scan - finds disabled mod", "[scan]") {
    Fixture fx;
    fx.installDisabled("0005000010101000", "my-mod", "1.0.0");

    auto mods = InstalledScanner::scan();
    REQUIRE(mods.size() == 1);
    REQUIRE(mods[0].id == "my-mod");
    REQUIRE_FALSE(mods[0].active);
}

TEST_CASE("InstalledScanner::scan - finds active and disabled", "[scan]") {
    Fixture fx;
    fx.installActive("000500001010EB00",  "mario-active", "1.0");
    fx.installDisabled("000500001010EC00", "luigi-disabled", "2.0");

    auto mods = InstalledScanner::scan();
    REQUIRE(mods.size() == 2);

    auto* active   = findMod(mods, "mario-active");
    auto* disabled = findMod(mods, "luigi-disabled");
    REQUIRE(active   != nullptr);
    REQUIRE(disabled != nullptr);
    REQUIRE(active->active);
    REQUIRE_FALSE(disabled->active);
}

TEST_CASE("InstalledScanner::scan - mod without modinfo.json still listed", "[scan]") {
    Fixture fx;
    std::string dir = Paths::sdcafiineBase() + "/0005000010101000/no-info";
    mkdirs(dir);

    auto mods = InstalledScanner::scan();
    REQUIRE(mods.size() == 1);
    REQUIRE(mods[0].id == "no-info");
    REQUIRE(mods[0].name == "no-info"); // falls back to id
    REQUIRE(mods[0].version.empty());
}

TEST_CASE("InstalledScanner::scan - corrupt modinfo.json doesn't crash", "[scan]") {
    Fixture fx;
    std::string dir = Paths::sdcafiineBase() + "/0005000010101000/corrupt-mod";
    mkdirs(dir);
    writeFile(dir + "/modinfo.json", "{not valid json");

    auto mods = InstalledScanner::scan();
    REQUIRE(mods.size() == 1);
    REQUIRE(mods[0].version.empty());
}

TEST_CASE("InstalledScanner::scan - multiple titleIds and mods", "[scan]") {
    Fixture fx;
    fx.installActive("000500001010EB00", "mod-jp1", "1.0");
    fx.installActive("000500001010EB00", "mod-jp2", "2.0");
    fx.installActive("000500001010EC00", "mod-us1", "1.0");

    auto mods = InstalledScanner::scan();
    REQUIRE(mods.size() == 3);
}

TEST_CASE("InstalledScanner::scan - path field is set correctly", "[scan]") {
    Fixture fx;
    fx.installActive("0005000010101000", "my-mod", "1.0");

    auto mods = InstalledScanner::scan();
    REQUIRE(mods.size() == 1);
    REQUIRE(mods[0].path ==
            Paths::sdcafiineBase() + "/0005000010101000/my-mod");
}

// ─── setActive ────────────────────────────────────────────────────────────

TEST_CASE("InstalledScanner::setActive - active -> disabled moves folder", "[active]") {
    Fixture fx;
    fx.installActive("0005000010101000", "my-mod", "1.0");

    auto mods = InstalledScanner::scan();
    REQUIRE(mods.size() == 1);
    InstalledMod mod = mods[0];

    REQUIRE(InstalledScanner::setActive(mod, false));
    REQUIRE_FALSE(mod.active);
    REQUIRE(mod.path == Paths::disabledBase() + "/0005000010101000/my-mod");

    // Verify on disk
    REQUIRE_FALSE(exists(Paths::sdcafiineBase() + "/0005000010101000/my-mod"));
    REQUIRE(exists(Paths::disabledBase() + "/0005000010101000/my-mod"));
    REQUIRE(exists(Paths::disabledBase() + "/0005000010101000/my-mod/modinfo.json"));
}

TEST_CASE("InstalledScanner::setActive - disabled -> active moves folder", "[active]") {
    Fixture fx;
    fx.installDisabled("0005000010101000", "my-mod", "1.0");

    auto mods = InstalledScanner::scan();
    REQUIRE(mods.size() == 1);
    InstalledMod mod = mods[0];

    REQUIRE(InstalledScanner::setActive(mod, true));
    REQUIRE(mod.active);
    REQUIRE(mod.path == Paths::sdcafiineBase() + "/0005000010101000/my-mod");

    REQUIRE(exists(Paths::sdcafiineBase() + "/0005000010101000/my-mod"));
    REQUIRE_FALSE(exists(Paths::disabledBase() + "/0005000010101000/my-mod"));
}

TEST_CASE("InstalledScanner::setActive - missing source returns false", "[active]") {
    Fixture fx;
    InstalledMod mod;
    mod.titleId = "0005000010101000";
    mod.id      = "ghost-mod";
    mod.active  = true;
    mod.path    = Paths::sdcafiineBase() + "/0005000010101000/ghost-mod";

    REQUIRE_FALSE(InstalledScanner::setActive(mod, false));
}

TEST_CASE("InstalledScanner::setActive - creates target titleId folder if missing", "[active]") {
    Fixture fx;
    fx.installActive("0005000010101000", "my-mod", "1.0");
    // Disabled base titleId folder doesn't exist yet -- setActive must create it

    auto mods = InstalledScanner::scan();
    InstalledMod mod = mods[0];

    REQUIRE(InstalledScanner::setActive(mod, false));
    REQUIRE(exists(Paths::disabledBase() + "/0005000010101000"));
}

TEST_CASE("InstalledScanner::setActive - roundtrip active -> disabled -> active", "[active]") {
    Fixture fx;
    fx.installActive("0005000010101000", "my-mod", "1.0");

    auto mods = InstalledScanner::scan();
    InstalledMod mod = mods[0];

    REQUIRE(InstalledScanner::setActive(mod, false));
    REQUIRE_FALSE(mod.active);
    REQUIRE(InstalledScanner::setActive(mod, true));
    REQUIRE(mod.active);

    REQUIRE(exists(Paths::sdcafiineBase() + "/0005000010101000/my-mod/modinfo.json"));
    REQUIRE_FALSE(exists(Paths::disabledBase() + "/0005000010101000/my-mod"));
}

// ─── remove ───────────────────────────────────────────────────────────────

TEST_CASE("InstalledScanner::remove - active mod is removed from disk", "[remove]") {
    Fixture fx;
    fx.installActive("0005000010101000", "my-mod", "1.0");

    auto mods = InstalledScanner::scan();
    InstalledMod mod = mods[0];

    REQUIRE(InstalledScanner::remove(mod));
    REQUIRE_FALSE(exists(mod.path));
}

TEST_CASE("InstalledScanner::remove - disabled mod is removed from disk", "[remove]") {
    Fixture fx;
    fx.installDisabled("0005000010101000", "my-mod", "1.0");

    auto mods = InstalledScanner::scan();
    InstalledMod mod = mods[0];

    REQUIRE(InstalledScanner::remove(mod));
    REQUIRE_FALSE(exists(mod.path));
}

TEST_CASE("InstalledScanner::remove - removes nested mod folder contents", "[remove]") {
    Fixture fx;
    std::string dir = Paths::sdcafiineBase() + "/0005000010101000/my-mod";
    mkdirs(dir + "/content/sub");
    writeFile(dir + "/modinfo.json", "{}");
    writeFile(dir + "/content/file.szs", "x");
    writeFile(dir + "/content/sub/deep.txt", "y");

    InstalledMod mod;
    mod.path = dir;

    REQUIRE(InstalledScanner::remove(mod));
    REQUIRE_FALSE(exists(dir));
}

TEST_CASE("InstalledScanner::remove - missing path returns true (no-op)", "[remove]") {
    Fixture fx;
    InstalledMod mod;
    mod.path = "/tmp/this_path_xyz_does_not_exist_123";
    // remove() returns rmrf which returns true for missing paths
    REQUIRE(InstalledScanner::remove(mod));
}

// ─── Phase 5: rmrf hardening ──────────────────────────────────────────────

TEST_CASE("InstalledScanner::remove - symlink loop does not hang", "[remove][security]") {
    Fixture fx;
    std::string dir = Paths::sdcafiineBase() + "/0005000010101000/loopy";
    mkdirs(dir);
    writeFile(dir + "/modinfo.json", "{}");
    // Create symlink: dir/self -> dir (potential infinite loop without lstat)
    REQUIRE(symlink(dir.c_str(), (dir + "/self").c_str()) == 0);

    InstalledMod mod;
    mod.path = dir;

    // Should complete in reasonable time and clean up the directory
    REQUIRE(InstalledScanner::remove(mod));
    REQUIRE_FALSE(exists(dir));
}

TEST_CASE("InstalledScanner::remove - deep nesting handled up to depth limit",
          "[remove]") {
    Fixture fx;
    // Build moderately deep tree (10 levels) - well below cap (64)
    std::string base = Paths::sdcafiineBase() + "/0005000010101000/deep";
    std::string current = base;
    for (int i = 0; i < 10; i++) {
        mkdirs(current);
        writeFile(current + "/file.txt", "x");
        current += "/sub";
    }

    InstalledMod mod;
    mod.path = base;
    REQUIRE(InstalledScanner::remove(mod));
    REQUIRE_FALSE(exists(base));
}

TEST_CASE("InstalledScanner::setActive - stale target folder is cleared",
          "[active]") {
    Fixture fx;
    fx.installActive("0005000010101000", "my-mod", "1.0");
    // Pre-create a stale disabled/<titleId>/<modId> dir to simulate a
    // previous failed deactivation
    std::string staleDst = Paths::disabledBase() + "/0005000010101000/my-mod";
    mkdirs(staleDst);
    writeFile(staleDst + "/old_file.txt", "stale");
    REQUIRE(exists(staleDst + "/old_file.txt"));

    auto mods = InstalledScanner::scan();
    InstalledMod mod = mods[0]; // active one
    REQUIRE(mod.active);

    REQUIRE(InstalledScanner::setActive(mod, false));
    // Stale target was cleared, fresh content is now there
    REQUIRE(exists(staleDst + "/modinfo.json"));
    REQUIRE_FALSE(exists(staleDst + "/old_file.txt"));
}
