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
    /* Counted rather than slept: the test is the one that decides when the
     * loop turns again. */
    void   rest() override;
    /* Counted, and the driven ones apart. */
    void   pause(bool driven) override;
    void   wake() override;
    /* 0 until offer_wake. */
    std::uint64_t wake_handle() const override;
    std::uint64_t woken() const override;
    /* Counted rather than slept, with what it was asked to wait past; runs
     * the hook first if there is one, which is the test playing whatever
     * happens on other threads meanwhile. */
    void          await_woken(std::uint64_t seen, std::uint64_t timeout) override;

    /* The process has a driver wake to hand its contexts, by this handle. */
    void   offer_wake(std::uint64_t handle);
    /* The driver set the wake and the loop's wait saw it: woken() moves. */
    void   driver_woke();
    void   on_await(void (*hook)(void *context), void *context);

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
    std::uint32_t rests() const;
    std::uint32_t pauses() const;
    std::uint32_t driven_pauses() const;
    std::uint32_t wakes() const;
    std::uint32_t awaits() const;
    /* What the last await_woken was asked to wait past, and for how long. */
    std::uint64_t last_seen() const;
    std::uint64_t last_timeout() const;

private:
    icd::ServiceLoop *loop_;
    std::uint32_t     turns_;
    std::uint32_t     starts_;
    std::uint32_t     stops_;
    std::uint32_t     rests_;
    std::uint32_t     pauses_;
    std::uint32_t     driven_pauses_;
    std::uint32_t     wakes_;
    std::uint32_t     awaits_;
    std::uint64_t     wake_handle_;
    std::uint64_t     woken_;
    std::uint64_t     last_seen_;
    std::uint64_t     last_timeout_;
    void            (*await_hook_)(void *context);
    void             *await_context_;
    bool              stopping_;
    bool              refusing_;
};

}  // namespace av

#endif  // AV_SCRIPTED_THREAD_HPP
