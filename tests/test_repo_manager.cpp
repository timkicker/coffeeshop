#include <catch2/catch_test_macros.hpp>
#include "net/RepoManager.h"
#include <json.hpp>

// ─── validateUrl ────────────────────────────────────────────────────────────

TEST_CASE("validateUrl - valid https", "[repo]") { REQUIRE(RepoManager::validateUrl("https://example.com/repo.json")); }
TEST_CASE("validateUrl - valid http",  "[repo]") { REQUIRE(RepoManager::validateUrl("http://example.com/repo.json")); }
TEST_CASE("validateUrl - empty",       "[repo]") { REQUIRE_FALSE(RepoManager::validateUrl("")); }
TEST_CASE("validateUrl - no protocol", "[repo]") { REQUIRE_FALSE(RepoManager::validateUrl("example.com/repo.json")); }
TEST_CASE("validateUrl - ftp",         "[repo]") { REQUIRE_FALSE(RepoManager::validateUrl("ftp://example.com/repo.json")); }

// ─── parseGameFromJson ───────────────────────────────────────────────────────

static nlohmann::json validGame() {
    return nlohmann::json::parse(R"({
        "name": "Mario Kart 8",
        "titleIds": ["000500001010EB00", "000500001010EC00"],
        "mods": [
            {
                "id": "some-mod",
                "name": "Some Mod",
                "version": "1.0.0",
                "author": "Author",
                "download": "https://example.com/some-mod.zip",
                "type": "mod"
            }
        ]
    })");
}

TEST_CASE("parseGameFromJson - valid game parses correctly", "[repo]") {
    auto result = RepoManager::parseGameFromJson(validGame().dump());
    REQUIRE(result.has_value());
    REQUIRE(result->name == "Mario Kart 8");
    REQUIRE(result->titleIds.size() == 2);
    REQUIRE(result->mods.size() == 1);
    REQUIRE(result->mods[0].id == "some-mod");
    REQUIRE(result->mods[0].author == "Author");
}

TEST_CASE("parseGameFromJson - title_ids snake_case also accepted", "[repo]") {
    auto j = validGame();
    j.erase("titleIds");
    j["title_ids"] = {"000500001010EB00"};
    auto result = RepoManager::parseGameFromJson(j.dump());
    REQUIRE(result.has_value());
    REQUIRE(result->titleIds.size() == 1);
}

TEST_CASE("parseGameFromJson - missing name returns nullopt", "[repo]") {
    auto j = validGame();
    j.erase("name");
    REQUIRE_FALSE(RepoManager::parseGameFromJson(j.dump()).has_value());
}

TEST_CASE("parseGameFromJson - missing titleIds returns nullopt", "[repo]") {
    auto j = validGame();
    j.erase("titleIds");
    REQUIRE_FALSE(RepoManager::parseGameFromJson(j.dump()).has_value());
}

TEST_CASE("parseGameFromJson - missing mods returns nullopt", "[repo]") {
    auto j = validGame();
    j.erase("mods");
    REQUIRE_FALSE(RepoManager::parseGameFromJson(j.dump()).has_value());
}

TEST_CASE("parseGameFromJson - mod with invalid id is skipped", "[repo]") {
    auto j = validGame();
    j["mods"][0]["id"] = "Invalid ID!";
    // Only mod is skipped -> game has no mods -> nullopt
    REQUIRE_FALSE(RepoManager::parseGameFromJson(j.dump()).has_value());
}

TEST_CASE("parseGameFromJson - mod with invalid download URL is skipped", "[repo]") {
    auto j = validGame();
    j["mods"][0]["download"] = "not-a-url";
    REQUIRE_FALSE(RepoManager::parseGameFromJson(j.dump()).has_value());
}

TEST_CASE("parseGameFromJson - mod missing required field is skipped", "[repo]") {
    auto j = validGame();
    j["mods"][0].erase("version");
    REQUIRE_FALSE(RepoManager::parseGameFromJson(j.dump()).has_value());
}

TEST_CASE("parseGameFromJson - optional fields parsed correctly", "[repo]") {
    auto j = validGame();
    j["mods"][0]["releaseDate"] = "2024-03-15";
    j["mods"][0]["license"]     = "CC BY-NC";
    j["mods"][0]["fileSize"]    = 1048576;
    j["mods"][0]["tags"]        = {"skin", "music"};
    j["mods"][0]["requirements"] = {"60fps patch"};
    j["mods"][0]["changelog"]   = "1.0.0 - Initial release";
    j["icon"]                   = "https://example.com/icon.png";

    auto result = RepoManager::parseGameFromJson(j.dump());
    REQUIRE(result.has_value());
    auto& mod = result->mods[0];
    REQUIRE(mod.releaseDate  == "2024-03-15");
    REQUIRE(mod.license      == "CC BY-NC");
    REQUIRE(mod.fileSize     == 1048576);
    REQUIRE(mod.tags.size()  == 2);
    REQUIRE(mod.tags[0]      == "skin");
    REQUIRE(mod.requirements.size() == 1);
    REQUIRE(mod.changelog    == "1.0.0 - Initial release");
    REQUIRE(result->icon     == "https://example.com/icon.png");
}

TEST_CASE("parseGameFromJson - one valid and one invalid mod", "[repo]") {
    auto j = validGame();
    j["mods"].push_back({
        {"id", "bad mod!"},
        {"name", "Bad"},
        {"version", "1.0.0"},
        {"download", "https://example.com/bad.zip"}
    });
    // First mod is valid, second is skipped
    auto result = RepoManager::parseGameFromJson(j.dump());
    REQUIRE(result.has_value());
    REQUIRE(result->mods.size() == 1);
}

TEST_CASE("parseGameFromJson - unknown fields are ignored", "[repo]") {
    auto j = validGame();
    j["unknownField"]          = "whatever";
    j["mods"][0]["futureField"] = 42;
    REQUIRE(RepoManager::parseGameFromJson(j.dump()).has_value());
}

TEST_CASE("parseGameFromJson - modpack type parsed correctly", "[repo]") {
    auto j = validGame();
    j["mods"][0]["type"]     = "modpack";
    j["mods"][0]["includes"] = {"mod-a", "mod-b"};
    auto result = RepoManager::parseGameFromJson(j.dump());
    REQUIRE(result.has_value());
    REQUIRE(result->mods[0].type == "modpack");
    REQUIRE(result->mods[0].includes.size() == 2);
}
