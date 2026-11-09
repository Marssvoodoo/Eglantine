// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <future>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "common/thread_worker.h"

TEST_CASE("Worker completion includes captured resource cleanup", "[common][worker]") {
    std::promise<void> cleanup_started;
    auto started = cleanup_started.get_future();
    std::promise<void> cleanup_release;
    struct Resource {
        Resource(std::promise<void>& started_, std::shared_future<void> release_)
            : started{started_}, release{std::move(release_)} {}
        ~Resource() {
            started.set_value();
            release.wait();
        }
        std::promise<void>& started;
        std::shared_future<void> release;
    };
    Common::ThreadWorker workers{1, "CleanupTest"};
    auto resource = std::make_shared<Resource>(cleanup_started,
                                               cleanup_release.get_future().share());
    workers.QueueWork([resource = std::move(resource)] { (void)resource; });
    const bool cleaning = started.wait_for(std::chrono::seconds{2}) == std::future_status::ready;
    auto completed = std::async(std::launch::async, [&] { workers.WaitForRequests(); });
    const bool returned_during_cleanup =
        cleaning && completed.wait_for(std::chrono::milliseconds{50}) == std::future_status::ready;
    cleanup_release.set_value();
    completed.get();
    REQUIRE(cleaning);
    REQUIRE_FALSE(returned_during_cleanup);
}
