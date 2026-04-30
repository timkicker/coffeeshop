#include <catch2/catch_test_macros.hpp>
#include "app/Config.h"

#include <cstdio>
#include <fstream>
#include <unistd.h>

namespace {

static std::string tmpFile(const std::string& tag) {
    return std::string("/tmp/coffeeshop_cfg_") + tag + "_" +
           std::to_string(getpid()) + ".json";
}

static void writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    f << content;
}

} // namespace

TEST_CASE("Config::loadFrom - valid config with multiple repos", "[config]") {
    auto path = tmpFile("valid");
    writeFile(path, R"({
        "repos": ["https://example.com/repo.json", "https://example.org/repo.json"],
        "musicTrack": "main"
    })");

    Config c;
    REQUIRE(c.loadFrom(path));
    REQUIRE(c.repos.size() == 2);
    REQUIRE(c.repos[0] == "https://example.com/repo.json");
    REQUIRE(c.musicTrack == "main");
    remove(path.c_str());
}

TEST_CASE("Config::loadFrom - empty repos array", "[config]") {
    auto path = tmpFile("emptyrepos");
    writeFile(path, R"({"repos": []})");

    Config c;
    REQUIRE(c.loadFrom(path));
    REQUIRE(c.repos.empty());
    REQUIRE(c.musicTrack == "off"); // default
    remove(path.c_str());
}

TEST_CASE("Config::loadFrom - missing repos field", "[config]") {
    auto path = tmpFile("norepos");
    writeFile(path, R"({"musicTrack": "alt"})");

    Config c;
    REQUIRE(c.loadFrom(path));
    REQUIRE(c.repos.empty());
    REQUIRE(c.musicTrack == "alt");
    remove(path.c_str());
}

TEST_CASE("Config::loadFrom - missing file returns false", "[config]") {
    Config c;
    REQUIRE_FALSE(c.loadFrom("/tmp/this_config_does_not_exist_xyz.json"));
}

TEST_CASE("Config::loadFrom - malformed JSON returns false", "[config]") {
    auto path = tmpFile("bad");
    writeFile(path, "{not valid json");

    Config c;
    REQUIRE_FALSE(c.loadFrom(path));
    remove(path.c_str());
}

TEST_CASE("Config::loadFrom - non-string entries in repos array are skipped", "[config]") {
    auto path = tmpFile("mixedtype");
    writeFile(path, R"({"repos": ["https://ok.com/r.json", 42, null, "https://also.com/r.json"]})");

    Config c;
    REQUIRE(c.loadFrom(path));
    REQUIRE(c.repos.size() == 2);
    REQUIRE(c.repos[0] == "https://ok.com/r.json");
    REQUIRE(c.repos[1] == "https://also.com/r.json");
    remove(path.c_str());
}

TEST_CASE("Config::loadFrom - repos as non-array is ignored", "[config]") {
    auto path = tmpFile("nonarray");
    writeFile(path, R"({"repos": "not-an-array"})");

    Config c;
    REQUIRE(c.loadFrom(path));
    REQUIRE(c.repos.empty());
    remove(path.c_str());
}

TEST_CASE("Config::saveTo - writes valid JSON readable by loadFrom", "[config]") {
    auto path = tmpFile("roundtrip");

    Config c;
    c.repos      = {"https://a.com/r.json", "https://b.com/r.json"};
    c.musicTrack = "alt";
    REQUIRE(c.saveTo(path));

    Config c2;
    REQUIRE(c2.loadFrom(path));
    REQUIRE(c2.repos == c.repos);
    REQUIRE(c2.musicTrack == c.musicTrack);
    remove(path.c_str());
}

TEST_CASE("Config::saveTo - empty config produces valid JSON", "[config]") {
    auto path = tmpFile("empty_save");

    Config c;
    REQUIRE(c.saveTo(path));

    Config c2;
    REQUIRE(c2.loadFrom(path));
    REQUIRE(c2.repos.empty());
    REQUIRE(c2.musicTrack == "off");
    remove(path.c_str());
}

TEST_CASE("Config::saveTo - cannot write to non-existent directory", "[config]") {
    Config c;
    REQUIRE_FALSE(c.saveTo("/no/such/dir/cfg.json"));
}

TEST_CASE("Config::hasRepos - empty/non-empty", "[config]") {
    Config c;
    REQUIRE_FALSE(c.hasRepos());
    c.repos.push_back("https://example.com/r.json");
    REQUIRE(c.hasRepos());
}
