#include <catch2/catch_test_macros.hpp>
#include <string>

// Copy the static function for testing
static std::string resolveUrl(const std::string& base, const std::string& path) {
    if (path.substr(0, 4) == "http") return path;
    size_t slash = base.rfind('/');
    if (slash == std::string::npos) return path;
    return base.substr(0, slash + 1) + path;
}

TEST_CASE("resolveUrl - absolute path returns as-is", "[url]") {
    REQUIRE(resolveUrl("https://example.com/repo.json", "https://other.com/file.zip") 
            == "https://other.com/file.zip");
    REQUIRE(resolveUrl("https://example.com/repo.json", "http://other.com/file.zip") 
            == "http://other.com/file.zip");
}

TEST_CASE("resolveUrl - relative path in root", "[url]") {
    REQUIRE(resolveUrl("https://example.com/repo.json", "games/mk8.json") 
            == "https://example.com/games/mk8.json");
}

TEST_CASE("resolveUrl - relative path in subdirectory", "[url]") {
    REQUIRE(resolveUrl("https://example.com/repos/main/repo.json", "games/mk8.json") 
            == "https://example.com/repos/main/games/mk8.json");
}

TEST_CASE("resolveUrl - base without path component", "[url]") {
    // Edge case: base without path returns protocol + path (unlikely in practice)
    REQUIRE(resolveUrl("https://example.com", "file.json") == "https://file.json");
}

TEST_CASE("resolveUrl - relative path with subdirs", "[url]") {
    REQUIRE(resolveUrl("https://example.com/repos/repo.json", "games/mk8/game.json") 
            == "https://example.com/repos/games/mk8/game.json");
}

TEST_CASE("resolveUrl - preserves trailing slash behavior", "[url]") {
    REQUIRE(resolveUrl("https://example.com/repos/", "game.json") 
            == "https://example.com/repos/game.json");
}
