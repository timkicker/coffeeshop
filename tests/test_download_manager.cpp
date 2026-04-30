#include <catch2/catch_test_macros.hpp>
#include "net/DownloadManager.h"

#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>

namespace {

static std::string tmpPath(const std::string& tag) {
    return std::string("/tmp/coffeeshop_dm_") + tag + "_" + std::to_string(getpid());
}

static void writeBytes(const std::string& path, const std::vector<uint8_t>& bytes) {
    FILE* f = fopen(path.c_str(), "wb");
    REQUIRE(f != nullptr);
    if (!bytes.empty()) fwrite(bytes.data(), 1, bytes.size(), f);
    fclose(f);
}

static void touch(const std::string& path) {
    FILE* f = fopen(path.c_str(), "wb");
    if (f) fclose(f);
}

} // namespace

// ─── validateZip ───────────────────────────────────────────────────────────

TEST_CASE("DownloadManager::validateZip - valid ZIP signature passes", "[dm]") {
    auto path = tmpPath("validzip");
    // ZIP local file header signature: 0x04034b50, little-endian
    writeBytes(path, {0x50, 0x4b, 0x03, 0x04, 0, 0, 0, 0});
    REQUIRE(DownloadManager::validateZip(path));
    remove(path.c_str());
}

TEST_CASE("DownloadManager::validateZip - garbage bytes fail", "[dm]") {
    auto path = tmpPath("badzip");
    writeBytes(path, {'X', 'Y', 'Z', 'W', 0, 0, 0, 0});
    REQUIRE_FALSE(DownloadManager::validateZip(path));
    remove(path.c_str());
}

TEST_CASE("DownloadManager::validateZip - empty file fails", "[dm]") {
    auto path = tmpPath("emptyzip");
    writeBytes(path, {});
    REQUIRE_FALSE(DownloadManager::validateZip(path));
    remove(path.c_str());
}

TEST_CASE("DownloadManager::validateZip - file with <4 bytes fails", "[dm]") {
    auto path = tmpPath("tinyzip");
    writeBytes(path, {0x50, 0x4b}); // only 2 bytes
    REQUIRE_FALSE(DownloadManager::validateZip(path));
    remove(path.c_str());
}

TEST_CASE("DownloadManager::validateZip - missing file fails", "[dm]") {
    REQUIRE_FALSE(DownloadManager::validateZip("/tmp/this_definitely_does_not_exist.zip"));
}

// ─── rmrf ──────────────────────────────────────────────────────────────────

TEST_CASE("DownloadManager::rmrf - removes single file", "[dm]") {
    auto path = tmpPath("rmrf_file");
    touch(path);
    struct stat st;
    REQUIRE(stat(path.c_str(), &st) == 0);

    REQUIRE(DownloadManager::rmrf(path));
    REQUIRE(stat(path.c_str(), &st) != 0); // gone
}

TEST_CASE("DownloadManager::rmrf - missing path returns true (no-op)", "[dm]") {
    REQUIRE(DownloadManager::rmrf("/tmp/this_path_does_not_exist_xyz_123"));
}

TEST_CASE("DownloadManager::rmrf - removes empty directory", "[dm]") {
    auto dir = tmpPath("rmrf_emptydir");
    mkdir(dir.c_str(), 0755);
    struct stat st;
    REQUIRE(stat(dir.c_str(), &st) == 0);

    REQUIRE(DownloadManager::rmrf(dir));
    REQUIRE(stat(dir.c_str(), &st) != 0);
}

TEST_CASE("DownloadManager::rmrf - removes nested directory tree", "[dm]") {
    auto root = tmpPath("rmrf_nested");
    mkdir(root.c_str(), 0755);
    mkdir((root + "/sub").c_str(), 0755);
    mkdir((root + "/sub/deep").c_str(), 0755);
    touch(root + "/file_root.txt");
    touch(root + "/sub/file_sub.txt");
    touch(root + "/sub/deep/file_deep.txt");

    REQUIRE(DownloadManager::rmrf(root));

    struct stat st;
    REQUIRE(stat(root.c_str(), &st) != 0);
}

TEST_CASE("DownloadManager::rmrf - directory with multiple files", "[dm]") {
    auto dir = tmpPath("rmrf_manyfiles");
    mkdir(dir.c_str(), 0755);
    for (int i = 0; i < 5; i++)
        touch(dir + "/file_" + std::to_string(i) + ".txt");

    REQUIRE(DownloadManager::rmrf(dir));

    struct stat st;
    REQUIRE(stat(dir.c_str(), &st) != 0);
}

// ─── checkDiskSpace ────────────────────────────────────────────────────────

TEST_CASE("DownloadManager::checkDiskSpace - 0 bytes always passes", "[dm]") {
    REQUIRE(DownloadManager::checkDiskSpace("/tmp", 0));
}

TEST_CASE("DownloadManager::checkDiskSpace - reasonable amount on tmp passes", "[dm]") {
    // 1 MB should be available basically everywhere
    REQUIRE(DownloadManager::checkDiskSpace("/tmp", 1024 * 1024));
}

TEST_CASE("DownloadManager::checkDiskSpace - missing path returns true (skip)", "[dm]") {
    // statvfs failure path: function logs warning and returns true.
    REQUIRE(DownloadManager::checkDiskSpace("/no/such/path/xyz_123", 1024));
}
