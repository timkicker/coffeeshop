#include <catch2/catch_test_macros.hpp>
#include "net/HttpClient.h"
#include "curl_mock.h"

#include <string>

TEST_CASE("HttpClient::get - 200 OK populates result", "[http]") {
    CurlMock::reset();
    CurlMock::setResponse("https://example.com/data", "hello", 200);

    std::string body;
    REQUIRE(HttpClient::get("https://example.com/data", body));
    REQUIRE(body == "hello");
}

TEST_CASE("HttpClient::get - 404 returns false and clears body", "[http]") {
    CurlMock::reset();
    CurlMock::setResponse("https://example.com/missing", "Not Found", 404);

    std::string body;
    REQUIRE_FALSE(HttpClient::get("https://example.com/missing", body));
    REQUIRE(body.empty());
}

TEST_CASE("HttpClient::get - 500 returns false", "[http]") {
    CurlMock::reset();
    CurlMock::setResponse("https://example.com/oops", "Server Error", 500);

    std::string body;
    REQUIRE_FALSE(HttpClient::get("https://example.com/oops", body));
}

TEST_CASE("HttpClient::get - oversized body aborts", "[http]") {
    CurlMock::reset();
    std::string huge(11 * 1024 * 1024, 'A'); // > 10MB cap
    CurlMock::setResponse("https://example.com/huge", huge, 200);

    std::string body;
    REQUIRE_FALSE(HttpClient::get("https://example.com/huge", body));
}

TEST_CASE("HttpClient::get - curl-level error returns false", "[http]") {
    CurlMock::reset();
    CurlMock::setError("https://example.com/no-resolve", 6 /* CURLE_COULDNT_RESOLVE_HOST */);

    std::string body;
    REQUIRE_FALSE(HttpClient::get("https://example.com/no-resolve", body));
}

TEST_CASE("HttpClient::get - empty body still success at 200", "[http]") {
    CurlMock::reset();
    CurlMock::setResponse("https://example.com/empty", "", 200);

    std::string body;
    REQUIRE(HttpClient::get("https://example.com/empty", body));
    REQUIRE(body.empty());
}
