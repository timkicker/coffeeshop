#include <catch2/catch_test_macros.hpp>
#include "mods/InstallHelper.h"
#include "app/Paths.h"

#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <cstdio>

namespace {

static std::string testRoot() {
    return std::string("/tmp/coffeeshop_helper_") + std::to_string(getpid());
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
    void install(const std::string& titleId) {
        mkdirs(Paths::sdcafiineBase() + "/" + titleId);
    }
};

} // namespace


TEST_CASE("InstallHelper::regionName - JPN", "[region]") {
    REQUIRE(InstallHelper::regionName("000500001010EB00") == "JPN");
}

TEST_CASE("InstallHelper::regionName - USA", "[region]") {
    REQUIRE(InstallHelper::regionName("000500001010EC01") == "USA");
}

TEST_CASE("InstallHelper::regionName - EUR", "[region]") {
    REQUIRE(InstallHelper::regionName("000500001010ED02") == "EUR");
}

TEST_CASE("InstallHelper::regionName - AUS", "[region]") {
    REQUIRE(InstallHelper::regionName("000500001010EE03") == "AUS");
}

TEST_CASE("InstallHelper::regionName - KOR", "[region]") {
    REQUIRE(InstallHelper::regionName("000500001010EF04") == "KOR");
}

TEST_CASE("InstallHelper::regionName - CHN", "[region]") {
    REQUIRE(InstallHelper::regionName("000500001010F005") == "CHN");
}

TEST_CASE("InstallHelper::regionName - TWN", "[region]") {
    REQUIRE(InstallHelper::regionName("000500001010F106") == "TWN");
}

TEST_CASE("InstallHelper::regionName - unknown suffix returns suffix itself", "[region]") {
    REQUIRE(InstallHelper::regionName("00050000101099FF") == "FF");
}

TEST_CASE("InstallHelper::regionName - title ID shorter than 2 returns input", "[region]") {
    REQUIRE(InstallHelper::regionName("a") == "a");
    REQUIRE(InstallHelper::regionName("") == "");
}

TEST_CASE("InstallHelper::regionName - exactly 2 chars works", "[region]") {
    REQUIRE(InstallHelper::regionName("01") == "USA");
    REQUIRE(InstallHelper::regionName("00") == "JPN");
}

// ─── detectInstalled ──────────────────────────────────────────────────────

TEST_CASE("InstallHelper::detectInstalled - none installed returns all titleIds", "[detect]") {
    Fixture fx;
    auto found = InstallHelper::detectInstalled(
        {"000500001010EB00", "000500001010EC00", "000500001010ED00"});

    // Fallback: when none have folders, return all so user can choose
    REQUIRE(found.size() == 3);
}

TEST_CASE("InstallHelper::detectInstalled - filters to existing titleId folders", "[detect]") {
    Fixture fx;
    fx.install("000500001010EC00"); // only USA folder exists

    auto found = InstallHelper::detectInstalled(
        {"000500001010EB00", "000500001010EC00", "000500001010ED00"});

    REQUIRE(found.size() == 1);
    REQUIRE(found[0].id == "000500001010EC00");
    REQUIRE(found[0].region == "JPN"); // suffix 00 → JPN (titleId ending matters)
}

TEST_CASE("InstallHelper::detectInstalled - all installed returns all", "[detect]") {
    Fixture fx;
    fx.install("000500001010EB00");
    fx.install("000500001010EC00");

    auto found = InstallHelper::detectInstalled(
        {"000500001010EB00", "000500001010EC00"});

    REQUIRE(found.size() == 2);
}

TEST_CASE("InstallHelper::detectInstalled - empty input returns empty (no fallback)", "[detect]") {
    Fixture fx;
    auto found = InstallHelper::detectInstalled({});
    REQUIRE(found.empty());
}

TEST_CASE("InstallHelper::detectInstalled - region names are populated", "[detect]") {
    Fixture fx;
    auto found = InstallHelper::detectInstalled(
        {"000500001010EB00", "000500001010EC01", "000500001010ED02"});

    REQUIRE(found.size() == 3);
    // Order matches input
    REQUIRE(found[0].region == "JPN");
    REQUIRE(found[1].region == "USA");
    REQUIRE(found[2].region == "EUR");
}
