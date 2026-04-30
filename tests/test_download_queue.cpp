#include <catch2/catch_test_macros.hpp>
#include "net/DownloadQueue.h"

namespace {

static Mod makeMod(const std::string& id) {
    Mod m;
    m.id       = id;
    m.name     = "Test Mod " + id;
    m.version  = "1.0.0";
    m.author   = "Author";
    m.download = "https://example.com/" + id + ".zip";
    m.type     = "mod";
    return m;
}

static void resetQueue() {
    DownloadQueue::get()._testReset();
}

} // namespace

TEST_CASE("DownloadQueue::enqueue - jobs visible via jobs()", "[queue]") {
    resetQueue();
    auto& q = DownloadQueue::get();

    q.enqueue(makeMod("a"), "0005000010101000");
    q.enqueue(makeMod("b"), "0005000010101000");

    auto jobs = q.jobs();
    REQUIRE(jobs.size() == 2);
    REQUIRE(jobs[0].mod.id == "a");
    REQUIRE(jobs[1].mod.id == "b");
    REQUIRE(jobs[0].state == DownloadJob::State::Pending);
}

TEST_CASE("DownloadQueue::activeCount - counts pending jobs", "[queue]") {
    resetQueue();
    auto& q = DownloadQueue::get();

    REQUIRE(q.activeCount() == 0);
    q.enqueue(makeMod("a"), "tid");
    q.enqueue(makeMod("b"), "tid");
    REQUIRE(q.activeCount() == 2);
}

TEST_CASE("DownloadQueue::cancelJob - pending becomes Error 'Cancelled'", "[queue]") {
    resetQueue();
    auto& q = DownloadQueue::get();

    q.enqueue(makeMod("a"), "tid");
    q.cancelJob(0);

    auto jobs = q.jobs();
    REQUIRE(jobs.size() == 1);
    REQUIRE(jobs[0].state == DownloadJob::State::Error);
    REQUIRE(jobs[0].error == "Cancelled");
    REQUIRE(jobs[0].hasFinishedAt);
}

TEST_CASE("DownloadQueue::cancelJob - out-of-bounds index is no-op", "[queue]") {
    resetQueue();
    auto& q = DownloadQueue::get();

    q.enqueue(makeMod("a"), "tid");

    // Should not crash and should not affect existing jobs
    q.cancelJob(-1);
    q.cancelJob(99);

    auto jobs = q.jobs();
    REQUIRE(jobs.size() == 1);
    REQUIRE(jobs[0].state == DownloadJob::State::Pending);
}

TEST_CASE("DownloadQueue::cancelJob - empty queue is safe", "[queue]") {
    resetQueue();
    auto& q = DownloadQueue::get();

    q.cancelJob(0);
    REQUIRE(q.jobs().empty());
}

TEST_CASE("DownloadQueue::cancelJob - idempotent on already-cancelled job", "[queue]") {
    resetQueue();
    auto& q = DownloadQueue::get();

    q.enqueue(makeMod("a"), "tid");
    q.cancelJob(0);
    q.cancelJob(0); // Second call: state is Error, no transition possible

    auto jobs = q.jobs();
    REQUIRE(jobs.size() == 1);
    REQUIRE(jobs[0].state == DownloadJob::State::Error);
    REQUIRE(jobs[0].error == "Cancelled");
}

TEST_CASE("DownloadQueue::dismissError - removes Error job", "[queue]") {
    resetQueue();
    auto& q = DownloadQueue::get();

    q.enqueue(makeMod("a"), "tid");
    q.cancelJob(0); // -> Error

    q.dismissError(0);
    REQUIRE(q.jobs().empty());
}

TEST_CASE("DownloadQueue::dismissError - does not remove non-Error job", "[queue]") {
    resetQueue();
    auto& q = DownloadQueue::get();

    q.enqueue(makeMod("a"), "tid");

    q.dismissError(0); // job is Pending, not Error
    REQUIRE(q.jobs().size() == 1);
    REQUIRE(q.jobs()[0].state == DownloadJob::State::Pending);
}

TEST_CASE("DownloadQueue::dismissError - out-of-bounds is safe", "[queue]") {
    resetQueue();
    auto& q = DownloadQueue::get();

    q.dismissError(0);  // empty queue
    q.dismissError(-1);
    q.dismissError(99);

    REQUIRE(q.jobs().empty());
}

TEST_CASE("DownloadQueue::cancelJob - cancelling one of many leaves others Pending", "[queue]") {
    resetQueue();
    auto& q = DownloadQueue::get();

    q.enqueue(makeMod("a"), "tid");
    q.enqueue(makeMod("b"), "tid");
    q.enqueue(makeMod("c"), "tid");

    q.cancelJob(1); // cancel "b"

    auto jobs = q.jobs();
    REQUIRE(jobs.size() == 3);
    REQUIRE(jobs[0].state == DownloadJob::State::Pending);
    REQUIRE(jobs[1].state == DownloadJob::State::Error);
    REQUIRE(jobs[2].state == DownloadJob::State::Pending);
    REQUIRE(jobs[1].mod.id == "b");
}

TEST_CASE("DownloadQueue::dismissError - removing middle job shifts later indices", "[queue]") {
    resetQueue();
    auto& q = DownloadQueue::get();

    q.enqueue(makeMod("a"), "tid");
    q.enqueue(makeMod("b"), "tid");
    q.enqueue(makeMod("c"), "tid");

    q.cancelJob(1); // b -> Error
    q.dismissError(1);

    auto jobs = q.jobs();
    REQUIRE(jobs.size() == 2);
    REQUIRE(jobs[0].mod.id == "a");
    REQUIRE(jobs[1].mod.id == "c");
}

TEST_CASE("DownloadQueue::activeCount - excludes Done and Error", "[queue]") {
    resetQueue();
    auto& q = DownloadQueue::get();

    q.enqueue(makeMod("a"), "tid");
    q.enqueue(makeMod("b"), "tid");
    q.cancelJob(1); // b -> Error

    REQUIRE(q.activeCount() == 1); // only "a" pending
}
