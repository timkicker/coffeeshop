#include <catch2/catch_test_macros.hpp>
#include "net/RepoManager.h"
#include "curl_mock.h"

#include <atomic>

namespace {

// Minimal valid game JSON, stringified for embedding in repo bodies.
static const char* kValidGameInline = R"({
    "name": "Mario Kart 8",
    "titleIds": ["000500001010EB00"],
    "mods": [{
        "id": "test-mod",
        "name": "Test Mod",
        "version": "1.0",
        "author": "A",
        "download": "https://example.com/test.zip",
        "type": "mod"
    }]
})";

} // namespace

TEST_CASE("RepoManager::fetch - inline games parsed from repo body", "[fetch]") {
    CurlMock::reset();
    CurlMock::setResponse("https://repo.example.com/repo.json", std::string(R"({
        "name": "Test Repo",
        "formatVersion": 1,
        "games": [)") + kValidGameInline + "]}");

    RepoManager rm;
    rm.fetch("https://repo.example.com/repo.json");

    REQUIRE(rm.lastError().empty());
    REQUIRE(rm.repo().name == "Test Repo");
    REQUIRE(rm.repo().games.size() == 1);
    REQUIRE(rm.repo().games[0].name == "Mario Kart 8");
    REQUIRE(rm.repo().games[0].mods.size() == 1);
    REQUIRE(rm.repo().games[0].mods[0].id == "test-mod");
}

TEST_CASE("RepoManager::fetch - games with path field follow-fetched", "[fetch]") {
    CurlMock::reset();
    CurlMock::setResponse("https://repo.example.com/repo.json", R"({
        "name": "Test Repo",
        "games": [{ "path": "games/mk8.json" }]
    })");
    CurlMock::setResponse("https://repo.example.com/games/mk8.json", kValidGameInline);

    RepoManager rm;
    rm.fetch("https://repo.example.com/repo.json");

    REQUIRE(rm.lastError().empty());
    REQUIRE(rm.repo().games.size() == 1);
    REQUIRE(rm.repo().games[0].name == "Mario Kart 8");
    REQUIRE(CurlMock::performCount() == 2); // repo.json + game.json
}

TEST_CASE("RepoManager::fetch - mixed inline and follow-fetch", "[fetch]") {
    CurlMock::reset();
    CurlMock::setResponse("https://repo.example.com/repo.json", std::string(R"({
        "games": [)") + kValidGameInline +
        R"(, { "path": "games/other.json" }]
    })");
    CurlMock::setResponse("https://repo.example.com/games/other.json", R"({
        "name": "Other",
        "titleIds": ["000500001010F100"],
        "mods": [{
            "id": "other-mod", "name": "Other", "version": "1.0",
            "download": "https://example.com/other.zip"
        }]
    })");

    RepoManager rm;
    rm.fetch("https://repo.example.com/repo.json");

    REQUIRE(rm.repo().games.size() == 2);
}

TEST_CASE("RepoManager::fetch - failed game.json fetch is skipped, others still parsed", "[fetch]") {
    CurlMock::reset();
    CurlMock::setResponse("https://repo.example.com/repo.json", std::string(R"({
        "games": [
            { "path": "games/missing.json" },)") + kValidGameInline + "]}");
    // No mapping for missing.json → simulated 404/connect-failure
    // valid game still parses inline

    RepoManager rm;
    rm.fetch("https://repo.example.com/repo.json");

    REQUIRE(rm.repo().games.size() == 1);
    REQUIRE(rm.repo().games[0].name == "Mario Kart 8");
}

TEST_CASE("RepoManager::fetch - rejects newer formatVersion than supported", "[fetch]") {
    CurlMock::reset();
    CurlMock::setResponse("https://repo.example.com/repo.json", std::string(R"({
        "formatVersion": 99,
        "games": [)") + kValidGameInline + "]}");

    RepoManager rm;
    rm.fetch("https://repo.example.com/repo.json");

    REQUIRE_FALSE(rm.lastError().empty());
    REQUIRE(rm.repo().games.empty());
}

TEST_CASE("RepoManager::fetch - older formatVersion still parses with warning", "[fetch]") {
    CurlMock::reset();
    CurlMock::setResponse("https://repo.example.com/repo.json", std::string(R"({
        "formatVersion": 0,
        "games": [)") + kValidGameInline + "]}");

    RepoManager rm;
    rm.fetch("https://repo.example.com/repo.json");

    // Lenient: older format parses, just logs a warning
    REQUIRE(rm.repo().games.size() == 1);
}

TEST_CASE("RepoManager::fetch - missing games array sets error", "[fetch]") {
    CurlMock::reset();
    CurlMock::setResponse("https://repo.example.com/repo.json", R"({
        "name": "Empty Repo"
    })");

    RepoManager rm;
    rm.fetch("https://repo.example.com/repo.json");

    REQUIRE_FALSE(rm.lastError().empty());
}

TEST_CASE("RepoManager::fetch - games is non-array sets error", "[fetch]") {
    CurlMock::reset();
    CurlMock::setResponse("https://repo.example.com/repo.json", R"({
        "games": "oops"
    })");

    RepoManager rm;
    rm.fetch("https://repo.example.com/repo.json");

    REQUIRE_FALSE(rm.lastError().empty());
}

TEST_CASE("RepoManager::fetch - malformed JSON sets error", "[fetch]") {
    CurlMock::reset();
    CurlMock::setResponse("https://repo.example.com/repo.json", "{not valid json");

    RepoManager rm;
    rm.fetch("https://repo.example.com/repo.json");

    REQUIRE_FALSE(rm.lastError().empty());
}

TEST_CASE("RepoManager::fetch - network error surfaces as error", "[fetch]") {
    CurlMock::reset();
    CurlMock::setError("https://repo.example.com/repo.json", 6 /* CURLE_COULDNT_RESOLVE_HOST */);

    RepoManager rm;
    rm.fetch("https://repo.example.com/repo.json");

    REQUIRE_FALSE(rm.lastError().empty());
    REQUIRE(rm.lastError().find("Network") != std::string::npos);
}

TEST_CASE("RepoManager::fetch - HTTP 404 surfaces as error", "[fetch]") {
    CurlMock::reset();
    CurlMock::setResponse("https://repo.example.com/repo.json", "Not Found", 404);

    RepoManager rm;
    rm.fetch("https://repo.example.com/repo.json");

    REQUIRE_FALSE(rm.lastError().empty());
}

TEST_CASE("RepoManager::fetch - empty response surfaces as error", "[fetch]") {
    CurlMock::reset();
    CurlMock::setResponse("https://repo.example.com/repo.json", "");

    RepoManager rm;
    rm.fetch("https://repo.example.com/repo.json");

    REQUIRE_FALSE(rm.lastError().empty());
}

TEST_CASE("RepoManager::fetch - invalid URL fails before any fetch", "[fetch]") {
    CurlMock::reset();
    RepoManager rm;
    rm.fetch("not a url");

    REQUIRE_FALSE(rm.lastError().empty());
    REQUIRE(CurlMock::performCount() == 0); // never even tried
}

TEST_CASE("RepoManager::fetch - empty games array reports no valid mods", "[fetch]") {
    CurlMock::reset();
    CurlMock::setResponse("https://repo.example.com/repo.json", R"({
        "games": []
    })");

    RepoManager rm;
    rm.fetch("https://repo.example.com/repo.json");

    REQUIRE_FALSE(rm.lastError().empty());
    REQUIRE(rm.repo().games.empty());
}

TEST_CASE("RepoManager::fetch - non-string path field is skipped", "[fetch]") {
    CurlMock::reset();
    // path is a number, not a string -- used to throw nlohmann::json::exception
    CurlMock::setResponse("https://repo.example.com/repo.json", R"({
        "games": [{ "path": 42 }]
    })");

    RepoManager rm;
    rm.fetch("https://repo.example.com/repo.json");

    // Should not crash; should report no valid mods
    REQUIRE(rm.repo().games.empty());
    REQUIRE_FALSE(rm.lastError().empty());
}

TEST_CASE("RepoManager::fetch - all-game-fetches-failed surfaces specific error", "[fetch]") {
    CurlMock::reset();
    CurlMock::setResponse("https://repo.example.com/repo.json", R"({
        "games": [
            { "path": "games/a.json" },
            { "path": "games/b.json" }
        ]
    })");
    // Neither game.json registered → all fetches fail
    RepoManager rm;
    rm.fetch("https://repo.example.com/repo.json");

    REQUIRE(rm.repo().games.empty());
    REQUIRE_FALSE(rm.lastError().empty());
    // Error message should mention that all game fetches failed, not just
    // the generic "no valid mods"
    REQUIRE(rm.lastError().find("game fetches failed") != std::string::npos);
}

TEST_CASE("RepoManager::fetch - oversized response body is rejected", "[fetch]") {
    CurlMock::reset();
    // Build a body well over 10MB cap to trigger writeString abort
    std::string huge(11 * 1024 * 1024, 'X');
    CurlMock::setResponse("https://repo.example.com/repo.json", huge);

    RepoManager rm;
    rm.fetch("https://repo.example.com/repo.json");

    REQUIRE_FALSE(rm.lastError().empty());
    REQUIRE(rm.repo().games.empty());
}

TEST_CASE("RepoManager::fetch - second fetch resets repo state", "[fetch]") {
    CurlMock::reset();
    CurlMock::setResponse("https://r1.example.com/repo.json", std::string(R"({
        "name": "First",
        "games": [)") + kValidGameInline + "]}");
    CurlMock::setResponse("https://r2.example.com/repo.json", "{not json");

    RepoManager rm;
    rm.fetch("https://r1.example.com/repo.json");
    REQUIRE(rm.repo().games.size() == 1);

    rm.fetch("https://r2.example.com/repo.json");
    REQUIRE(rm.repo().games.empty()); // cleared
    REQUIRE(rm.repo().name.empty());
    REQUIRE_FALSE(rm.lastError().empty());
}
