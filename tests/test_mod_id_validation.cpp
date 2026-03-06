#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <string>

// Copy the validation logic for testing
static bool isValidModId(const std::string& id) {
    if (id.empty()) return false;
    return std::all_of(id.begin(), id.end(),
        [](char c){ return isalnum(c) || c == '-' || c == '_'; });
}

TEST_CASE("Mod ID - valid alphanumeric", "[modid]") {
    REQUIRE(isValidModId("mymod123"));
    REQUIRE(isValidModId("UPPERCASE"));
    REQUIRE(isValidModId("123numbers"));
}

TEST_CASE("Mod ID - valid with dashes", "[modid]") {
    REQUIRE(isValidModId("my-mod"));
    REQUIRE(isValidModId("some-long-mod-name"));
    REQUIRE(isValidModId("-leading-dash"));
    REQUIRE(isValidModId("trailing-dash-"));
}

TEST_CASE("Mod ID - valid with underscores", "[modid]") {
    REQUIRE(isValidModId("my_mod"));
    REQUIRE(isValidModId("some_long_mod_name"));
    REQUIRE(isValidModId("_leading_underscore"));
    REQUIRE(isValidModId("trailing_underscore_"));
}

TEST_CASE("Mod ID - mixed valid characters", "[modid]") {
    REQUIRE(isValidModId("mod-123_test"));
    REQUIRE(isValidModId("a1-b2_c3"));
}

TEST_CASE("Mod ID - invalid spaces", "[modid]") {
    REQUIRE_FALSE(isValidModId("my mod"));
    REQUIRE_FALSE(isValidModId("mod name with spaces"));
}

TEST_CASE("Mod ID - invalid special chars", "[modid]") {
    REQUIRE_FALSE(isValidModId("mod!"));
    REQUIRE_FALSE(isValidModId("mod@name"));
    REQUIRE_FALSE(isValidModId("mod#test"));
    REQUIRE_FALSE(isValidModId("mod$"));
    REQUIRE_FALSE(isValidModId("mod%"));
    REQUIRE_FALSE(isValidModId("mod.test"));
    REQUIRE_FALSE(isValidModId("mod/test"));
    REQUIRE_FALSE(isValidModId("mod\\test"));
}

TEST_CASE("Mod ID - empty string", "[modid]") {
    REQUIRE_FALSE(isValidModId(""));
}

TEST_CASE("Mod ID - single character", "[modid]") {
    REQUIRE(isValidModId("a"));
    REQUIRE(isValidModId("1"));
    REQUIRE(isValidModId("-"));
    REQUIRE(isValidModId("_"));
}
