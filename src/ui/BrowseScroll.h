#pragma once

// Pure scroll-window math for the Browse-tab card grid.
//
// Inputs:
//   currentScroll  top row index currently visible
//   selectedRow    row index of the cursor (0-based)
//   visibleRows    how many rows fit on screen (must be >= 1)
// Returns: the new scroll value that keeps `selectedRow` in view with
// minimal movement (no scroll if already visible).
//
// Extracted for unit testing; the actual MainLayout::handleBrowseInput
// just calls this and assigns the result.
inline int computeBrowseScroll(int currentScroll, int selectedRow, int visibleRows) {
    if (visibleRows < 1) visibleRows = 1;
    if (currentScroll < 0) currentScroll = 0;
    if (selectedRow < currentScroll) {
        return selectedRow;
    }
    int lastVisible = currentScroll + visibleRows - 1;
    if (selectedRow > lastVisible) {
        return selectedRow - visibleRows + 1;
    }
    return currentScroll;
}
