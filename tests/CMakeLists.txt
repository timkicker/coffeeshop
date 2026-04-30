cmake_minimum_required(VERSION 3.14)
project(cupstore_tests CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)
# Source files under test (pure logic only - no WUT/SDL/curl)
set(SRC ${CMAKE_SOURCE_DIR}/../src)
set(LIBS ${CMAKE_SOURCE_DIR}/../libs)
add_executable(cupstore_tests
    test_conflict_checker.cpp
    test_repo_manager.cpp
    test_install_checker.cpp
    test_installed_scanner.cpp
    test_url_resolver.cpp
    test_mod_id_validation.cpp
    test_zip_extractor.cpp
    test_download_manager.cpp
    test_download_queue.cpp
    test_config.cpp
    test_cache_manager.cpp
    test_install_helper.cpp
    test_repo_manager_fetch.cpp
    test_integration.cpp
    curl_mock.cpp
    stubs.cpp
    ${SRC}/mods/ConflictChecker.cpp
    ${SRC}/net/RepoManager.cpp
    ${SRC}/mods/InstallChecker.cpp
    ${SRC}/mods/InstalledScanner.cpp
    ${SRC}/mods/InstallHelper.cpp
    ${SRC}/mods/ZipExtractor.cpp
    ${SRC}/net/DownloadManager.cpp
    ${SRC}/net/DownloadQueue.cpp
    ${SRC}/app/Config.cpp
    ${SRC}/app/CacheManager.cpp
)
target_include_directories(cupstore_tests PRIVATE
    ${SRC}
    ${LIBS}
)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
target_link_libraries(cupstore_tests PRIVATE
    Catch2::Catch2WithMain
    ZLIB::ZLIB
    Threads::Threads
)
# Stub out filesystem paths for tests
target_compile_definitions(cupstore_tests PRIVATE
    CUPSTORE_TESTS=1
    BUILD_HW=0
)
include(CTest)
include(Catch)
catch_discover_tests(cupstore_tests)
