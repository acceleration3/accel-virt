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
    void   rest() override;
    void   pause(bool driven) override;
    void   wake() override;
    std::uint64_t wake_handle() const override;
    std::uint64_t woken() const override;
    void          await_woken(std::uint64_t seen, std::uint64_t timeout) override;

    void   offer_wake(std::uint64_t handle);
    void   driver_woke();
    void   on_await(void (*hook)(void *context), void *context);

    void   refuse_start();

    bool step();
    std::uint32_t run_until_done(std::uint32_t limit = 64u);

    std::uint32_t turns() const;
    std::uint32_t starts() const;
    std::uint32_t stops() const;
    std::uint32_t rests() const;
    std::uint32_t pauses() const;
    std::uint32_t driven_pauses() const;
    std::uint32_t wakes() const;
    std::uint32_t awaits() const;
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

}

#endif
