#include <catch2/catch_test_macros.hpp>
#include "mods/InstallChecker.h"
#include "app/Paths.h"

#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <cstdio>
#include <dirent.h>

namespace {

static std::string testRoot() {
    return std::string("/tmp/coffeeshop_install_") + std::to_string(getpid());
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

static void writeJson(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    f << content;
}

// Setup: configure Paths to point at our tmp dir, then create a mod folder
// at sdcafiine/<titleId>/<modId>/ with an optional modinfo.json.
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

    void installMod(const std::string& titleId, const std::string& modId,
                    const std::string& version, bool disabled = false) {
        std::string base = disabled ? Paths::disabledBase() : Paths::sdcafiineBase();
        std::string dir  = base + "/" + titleId + "/" + modId;
        mkdirs(dir);
        if (!version.empty())
            writeJson(dir + "/modinfo.json",
                      "{\"id\":\"" + modId + "\",\"version\":\"" + version + "\"}");
    }

    void installModWithoutModinfo(const std::string& titleId, const std::string& modId) {
        std::string dir = Paths::sdcafiineBase() + "/" + titleId + "/" + modId;
        mkdirs(dir);
    }
};

} // namespace

TEST_CASE("InstallChecker::check - mod not installed", "[install]") {
    Fixture fx;
    auto status = InstallChecker::check("some-mod", "1.0.0",
                                         {"0005000010101000"});
    REQUIRE_FALSE(status.installed);
    REQUIRE_FALSE(status.updateAvail);
}

TEST_CASE("InstallChecker::check - mod installed, same version, no update", "[install]") {
    Fixture fx;
    fx.installMod("0005000010101000", "my-mod", "1.0.0");

    auto status = InstallChecker::check("my-mod", "1.0.0",
                                         {"0005000010101000"});
    REQUIRE(status.installed);
    REQUIRE_FALSE(status.updateAvail);
    REQUIRE(status.installedVersion == "1.0.0");
    REQUIRE(status.titleId == "0005000010101000");
}

TEST_CASE("InstallChecker::check - mod installed, different version, update available", "[install]") {
    Fixture fx;
    fx.installMod("0005000010101000", "my-mod", "1.0.0");

    auto status = InstallChecker::check("my-mod", "1.1.0",
                                         {"0005000010101000"});
    REQUIRE(status.installed);
    REQUIRE(status.updateAvail);
    REQUIRE(status.installedVersion == "1.0.0");
}

TEST_CASE("InstallChecker::check - mod folder exists but no modinfo.json", "[install]") {
    Fixture fx;
    fx.installModWithoutModinfo("0005000010101000", "my-mod");

    auto status = InstallChecker::check("my-mod", "1.0.0",
                                         {"0005000010101000"});
    REQUIRE(status.installed);
    REQUIRE(status.installedVersion.empty());
    // updateAvail requires a known installed version, so should be false here
    REQUIRE_FALSE(status.updateAvail);
}

TEST_CASE("InstallChecker::check - finds mod in disabled folder", "[install]") {
    Fixture fx;
    fx.installMod("0005000010101000", "my-mod", "1.0.0", /*disabled=*/true);

    auto status = InstallChecker::check("my-mod", "1.0.0",
                                         {"0005000010101000"});
    REQUIRE(status.installed);
    REQUIRE(status.titleId == "0005000010101000");
}

TEST_CASE("InstallChecker::check - searches multiple titleIds", "[install]") {
    Fixture fx;
    // Mod installed under EUR titleId, but query lists JPN/USA/EUR
    fx.installMod("000500001010ED00", "kart-mod", "2.0.0");

    auto status = InstallChecker::check("kart-mod", "2.0.0", {
        "000500001010EB00", "000500001010EC00", "000500001010ED00"
    });
    REQUIRE(status.installed);
    REQUIRE(status.titleId == "000500001010ED00");
}

TEST_CASE("InstallChecker::check - empty repoVersion never reports update", "[install]") {
    Fixture fx;
    fx.installMod("0005000010101000", "my-mod", "1.0.0");

    auto status = InstallChecker::check("my-mod", "", {"0005000010101000"});
    REQUIRE(status.installed);
    REQUIRE_FALSE(status.updateAvail); // unknown repoVersion -> can't tell
}

TEST_CASE("InstallChecker::check - corrupt modinfo.json (invalid JSON)", "[install]") {
    Fixture fx;
    std::string dir = Paths::sdcafiineBase() + "/0005000010101000/my-mod";
    mkdirs(dir);
    writeJson(dir + "/modinfo.json", "{not valid json");

    auto status = InstallChecker::check("my-mod", "1.0.0",
                                         {"0005000010101000"});
    REQUIRE(status.installed);
    REQUIRE(status.installedVersion.empty()); // parse failed silently
}
