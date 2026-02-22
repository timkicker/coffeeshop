#include <catch2/catch_test_macros.hpp>
#include "mods/InstalledScanner.h"

// Test hasUpdate which is pure string comparison
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
    // We do simple string inequality - version ordering not implemented
    InstalledMod mod;
    mod.version     = "1.1.0";
    mod.repoVersion = "1.0.0";
    REQUIRE(InstalledScanner::hasUpdate(mod)); // differs = update flagged
}
