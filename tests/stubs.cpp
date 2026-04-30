#include "app/Paths.h"

// Paths::sdMounted is a static member - needs a definition in exactly one TU
bool Paths::sdMounted = false;
std::string Paths::testRootOverride;

// curl stubs are provided by curl_mock.cpp (real-ish behavior driven by tests)
