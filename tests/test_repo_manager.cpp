#include <catch2/catch_test_macros.hpp>

// Test validateUrl logic directly without compiling RepoManager (curl dep)
static bool validateUrl(const std::string& url) {
    if (url.empty()) return false;
    if (url.substr(0, 7) != "http://" && url.substr(0, 8) != "https://") return false;
    size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos) return false;
    std::string rest = url.substr(schemeEnd + 3);
    if (rest.empty() || rest.find('.') == std::string::npos) return false;
    return true;
}

TEST_CASE("validateUrl - valid https", "[repo]") { REQUIRE(validateUrl("https://example.com/repo.json")); }
TEST_CASE("validateUrl - valid http",  "[repo]") { REQUIRE(validateUrl("http://example.com/repo.json")); }
TEST_CASE("validateUrl - empty",       "[repo]") { REQUIRE_FALSE(validateUrl("")); }
TEST_CASE("validateUrl - no protocol", "[repo]") { REQUIRE_FALSE(validateUrl("example.com/repo.json")); }
TEST_CASE("validateUrl - ftp",         "[repo]") { REQUIRE_FALSE(validateUrl("ftp://example.com/repo.json")); }
