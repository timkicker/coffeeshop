#include <catch2/catch_test_macros.hpp>
#include "ui/BrowseScroll.h"

// computeBrowseScroll(currentScroll, selectedRow, visibleRows) -> newScroll
//
// Property: after calling this, selectedRow is in [newScroll, newScroll + visibleRows - 1],
// and we move the window by the minimum amount necessary.

TEST_CASE("BrowseScroll: cursor already visible -> no scroll change", "[browse-scroll]") {
    REQUIRE(computeBrowseScroll(0, 0, 4) == 0);
    REQUIRE(computeBrowseScroll(0, 3, 4) == 0);
    REQUIRE(computeBrowseScroll(2, 2, 4) == 2);
    REQUIRE(computeBrowseScroll(2, 5, 4) == 2);
}

TEST_CASE("BrowseScroll: cursor below viewport -> scroll down minimally", "[browse-scroll]") {
    // visibleRows=4, scroll=0 means rows 0..3 visible. selectedRow=4 needs scroll=1.
    REQUIRE(computeBrowseScroll(0, 4, 4) == 1);
    // selectedRow=10 -> need rows 7..10 visible -> scroll=7
    REQUIRE(computeBrowseScroll(0, 10, 4) == 7);
}

TEST_CASE("BrowseScroll: cursor above viewport -> scroll up to expose it", "[browse-scroll]") {
    // scroll=5 visibleRows=4 means rows 5..8 visible. selectedRow=3 means jump up.
    REQUIRE(computeBrowseScroll(5, 3, 4) == 3);
    REQUIRE(computeBrowseScroll(10, 0, 4) == 0);
}

TEST_CASE("BrowseScroll: scrolling row-by-row matches expected window", "[browse-scroll]") {
    int scroll = 0;
    const int rows = 3;
    // Simulate cursor moving down 0..10
    for (int sel = 0; sel <= 10; sel++) {
        scroll = computeBrowseScroll(scroll, sel, rows);
        // sel must be visible
        REQUIRE(sel >= scroll);
        REQUIRE(sel <= scroll + rows - 1);
    }
    // After scrolling to row 10 with visibleRows=3, scroll should be 8
    REQUIRE(scroll == 8);
}

TEST_CASE("BrowseScroll: degenerate visibleRows clamped to 1", "[browse-scroll]") {
    REQUIRE(computeBrowseScroll(0, 5, 0) == 5);
    REQUIRE(computeBrowseScroll(0, 5, -1) == 5);
}

TEST_CASE("BrowseScroll: negative currentScroll is normalised", "[browse-scroll]") {
    // Shouldn't happen in practice, but ensure we don't underflow.
    REQUIRE(computeBrowseScroll(-3, 2, 4) == 0);  // row 2 visible from scroll=0
}

TEST_CASE("BrowseScroll: selectedRow == lastVisible edge", "[browse-scroll]") {
    // visibleRows=4 scroll=0 -> last visible = 3. row 3 should not move scroll.
    REQUIRE(computeBrowseScroll(0, 3, 4) == 0);
    // row 4 should bump scroll by 1.
    REQUIRE(computeBrowseScroll(0, 4, 4) == 1);
}
