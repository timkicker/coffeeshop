#include "app/Paths.h"
// Minimal stubs for test build
// Logger and Paths are header-only - no stubs needed
// curl is not linked in tests - stub the write callback if needed
bool Paths::sdMounted = false;
