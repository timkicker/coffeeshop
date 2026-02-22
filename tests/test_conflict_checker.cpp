#include <catch2/catch_test_macros.hpp>
#include "mods/ConflictChecker.h"

TEST_CASE("ConflictChecker::checkFiles - no conflict", "[conflict]") {
    std::vector<std::string> modA = {
        "content/driver/Mario.szs",
        "content/driver/Luigi.szs"
    };
    std::vector<ConflictChecker::ModFiles> others = {
        {"ModB", {"content/driver/Wario.szs", "content/driver/Waluigi.szs"}}
    };
    auto result = ConflictChecker::checkFiles(modA, others);
    REQUIRE_FALSE(result.hasConflict);
    REQUIRE(result.conflictingMods.empty());
    REQUIRE(result.conflictingFiles.empty());
}

TEST_CASE("ConflictChecker::checkFiles - direct conflict", "[conflict]") {
    std::vector<std::string> modA = {
        "content/driver/Mario.szs",
        "content/driver/Wario.szs"
    };
    std::vector<ConflictChecker::ModFiles> others = {
        {"ModB", {"content/driver/Wario.szs"}}
    };
    auto result = ConflictChecker::checkFiles(modA, others);
    REQUIRE(result.hasConflict);
    REQUIRE(result.conflictingMods.size() == 1);
    REQUIRE(result.conflictingMods[0] == "ModB");
    REQUIRE(result.conflictingFiles[0] == "content/driver/Wario.szs");
}

TEST_CASE("ConflictChecker::checkFiles - conflict with multiple mods", "[conflict]") {
    std::vector<std::string> modA = {
        "content/ui/main.bflyt",
        "content/driver/Mario.szs"
    };
    std::vector<ConflictChecker::ModFiles> others = {
        {"ModB", {"content/ui/main.bflyt"}},
        {"ModC", {"content/driver/Mario.szs"}}
    };
    auto result = ConflictChecker::checkFiles(modA, others);
    REQUIRE(result.hasConflict);
    REQUIRE(result.conflictingMods.size() == 2);
    REQUIRE(result.conflictingFiles.size() == 2);
}

TEST_CASE("ConflictChecker::checkFiles - max 3 conflicting files reported", "[conflict]") {
    std::vector<std::string> modA = {
        "content/a.szs", "content/b.szs", "content/c.szs", "content/d.szs"
    };
    std::vector<ConflictChecker::ModFiles> others = {
        {"ModB", {"content/a.szs", "content/b.szs", "content/c.szs", "content/d.szs"}}
    };
    auto result = ConflictChecker::checkFiles(modA, others);
    REQUIRE(result.hasConflict);
    REQUIRE(result.conflictingFiles.size() == 3); // capped at 3
}

TEST_CASE("ConflictChecker::checkFiles - empty mod files", "[conflict]") {
    auto result = ConflictChecker::checkFiles({}, {{"ModB", {"content/a.szs"}}});
    REQUIRE_FALSE(result.hasConflict);
}

TEST_CASE("ConflictChecker::checkFiles - no other mods", "[conflict]") {
    auto result = ConflictChecker::checkFiles({"content/a.szs"}, {});
    REQUIRE_FALSE(result.hasConflict);
}

TEST_CASE("ConflictChecker::checkFiles - duplicate mod name not reported twice", "[conflict]") {
    std::vector<std::string> modA = {"content/a.szs", "content/b.szs"};
    std::vector<ConflictChecker::ModFiles> others = {
        {"ModB", {"content/a.szs", "content/b.szs"}}
    };
    auto result = ConflictChecker::checkFiles(modA, others);
    REQUIRE(result.conflictingMods.size() == 1);
}
