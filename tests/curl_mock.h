#pragma once
#include <string>

// Test-side API for the curl mock (see curl_mock.cpp).
//
// Tests register URL -> response mappings before calling production code that
// invokes curl_easy_perform(). The mock matches on URL and either invokes the
// registered write callback with the body bytes (success) or returns an error
// curl code (failure). HTTP status is configurable.
namespace CurlMock {

// Clear all registered responses. Call between tests.
void reset();

// Register a successful response: GETs against `url` will produce `body` and
// return CURLE_OK with HTTP `httpCode`.
void setResponse(const std::string& url, const std::string& body, long httpCode = 200);

// Register a curl-level failure: GETs against `url` return `curlCode` from
// curl_easy_perform (e.g. 6 = CURLE_COULDNT_RESOLVE_HOST).
void setError(const std::string& url, int curlCode);

// How many times curl_easy_perform was invoked since last reset.
int performCount();

} // namespace CurlMock
