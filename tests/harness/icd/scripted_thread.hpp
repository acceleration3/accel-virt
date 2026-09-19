/* The service thread a test drives, on the test's own thread.
 *
 * Nothing else in this harness is threaded and the recording locks assert
 * single-thread ownership, so a test that started a real thread would be
 * asserting against itself. Instead the test says when the loop turns, which
 * is what makes an event watcher's behaviour something a test can state
 * exactly: this many turns, after this call, with this answer. */
#ifndef AV_SCRIPTED_THREAD_HPP
#define AV_SCRIPTED_THREAD_HPP

#include "icd/common/service_thread.hpp"

#include <cstdint>

namespace av {

class ScriptedThread final : public icd::ServiceThread {
public:
    ScriptedThread();

    Status start(icd::ServiceLoop &loop) override;
    void   stop() override;
    bool   running() const override;

    /* The next start refuses, as an OS out of threads does. What the ICD
     * does with that is a path of its own: a callback registered on a
     * watcher with no thread would never run. */
    void   refuse_start();

    /* Runs the loop once, as the thread would, and answers what it said.
     * False with nothing running. */
    bool step();
    /* Runs it until it says not to be called again, or `limit` turns have
     * gone by - which fails rather than hangs, as a real wait would. */
    std::uint32_t run_until_done(std::uint32_t limit = 64u);

    std::uint32_t turns() const;
    std::uint32_t starts() const;
    std::uint32_t stops() const;

private:
    icd::ServiceLoop *loop_;
    std::uint32_t     turns_;
    std::uint32_t     starts_;
    std::uint32_t     stops_;
    bool              stopping_;
    bool              refusing_;
};

}  // namespace av

#endif  // AV_SCRIPTED_THREAD_HPP
