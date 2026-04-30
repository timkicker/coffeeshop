#include <catch2/catch_test_macros.hpp>
#include "util/Logger.h"

#include <thread>
#include <vector>
#include <atomic>
#include <unistd.h>
#include <cstdio>

namespace {

static std::string tmpLogPath() {
    return std::string("/tmp/coffeeshop_logger_") + std::to_string(getpid()) + ".log";
}

} // namespace

TEST_CASE("Logger - concurrent writes don't crash or corrupt file", "[logger][thread]") {
    auto path = tmpLogPath();
    Logger::get().init(path);

    constexpr int kThreads = 8;
    constexpr int kPerThread = 200;

    std::atomic<int> ready{0};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; t++) {
        threads.emplace_back([t, &ready]() {
            ready++;
            for (int i = 0; i < kPerThread; i++) {
                LOG_INFO("thread=%d iter=%d msg=hello world payload", t, i);
            }
        });
    }
    for (auto& th : threads) th.join();

    // Read log back. Mutex should ensure each line is intact (starts with [INFO])
    // -- without mutex, fprintf interleaving would produce garbled lines.
    FILE* f = fopen(path.c_str(), "r");
    REQUIRE(f != nullptr);
    char line[1024];
    int totalLines = 0;
    int wellFormed = 0;
    while (fgets(line, sizeof(line), f) != nullptr) {
        totalLines++;
        // Each LOG_INFO line should start with "[INFO ]" or be the LOGGER READY header
        if (line[0] == '[' || std::string(line).find("LOGGER READY") == 0) wellFormed++;
    }
    fclose(f);

    REQUIRE(totalLines >= kThreads * kPerThread); // at least all our writes
    REQUIRE(wellFormed == totalLines);            // none got interleaved

    remove(path.c_str());
}
