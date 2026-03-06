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
    test_url_resolver.cpp
    test_mod_id_validation.cpp
    stubs.cpp
    ${SRC}/mods/ConflictChecker.cpp
    ${SRC}/net/RepoManager.cpp
    ${SRC}/mods/InstallChecker.cpp
    ${SRC}/mods/InstalledScanner.cpp
)
target_include_directories(cupstore_tests PRIVATE
    ${SRC}
    ${LIBS}
)
target_link_libraries(cupstore_tests PRIVATE Catch2::Catch2WithMain)
# Stub out filesystem paths for tests
target_compile_definitions(cupstore_tests PRIVATE
    CUPSTORE_TESTS=1
    BUILD_HW=0
)
include(CTest)
include(Catch)
catch_discover_tests(cupstore_tests)
