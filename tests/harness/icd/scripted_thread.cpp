#include "harness/icd/scripted_thread.hpp"

namespace av {

ScriptedThread::ScriptedThread()
    : loop_(nullptr), turns_(0u), starts_(0u), stops_(0u), rests_(0u), pauses_(0u), driven_pauses_(0u), wakes_(0u),
      awaits_(0u), wake_handle_(0u), woken_(0u), last_seen_(0u), last_timeout_(0u), await_hook_(nullptr),
      await_context_(nullptr), stopping_(false), refusing_(false)
{
}

Status ScriptedThread::start(icd::ServiceLoop &loop)
{
    if (refusing_) {
        refusing_ = false;
        return Status::no_memory;
    }
    if (loop_ != nullptr) return Status::invalid_state;
    loop_ = &loop;
    stopping_ = false;
    ++starts_;
    return Status::ok;
}

void ScriptedThread::stop()
{
    if (loop_ == nullptr) return;
    stopping_ = true;
    loop_ = nullptr;
    ++stops_;
}

bool ScriptedThread::running() const { return loop_ != nullptr; }

bool ScriptedThread::step()
{
    if (loop_ == nullptr) return false;
    ++turns_;
    const bool again = loop_->turn();
    if (!again) loop_ = nullptr;
    return again;
}

std::uint32_t ScriptedThread::run_until_done(std::uint32_t limit)
{
    std::uint32_t turned = 0u;
    while (turned < limit && step()) ++turned;
    return turned;
}

std::uint32_t ScriptedThread::turns() const { return turns_; }
std::uint32_t ScriptedThread::starts() const { return starts_; }
std::uint32_t ScriptedThread::stops() const { return stops_; }
std::uint32_t ScriptedThread::rests() const { return rests_; }
std::uint32_t ScriptedThread::pauses() const { return pauses_; }
std::uint32_t ScriptedThread::wakes() const { return wakes_; }
void ScriptedThread::rest() { ++rests_; }
void ScriptedThread::pause(bool driven)
{
    ++pauses_;
    if (driven) ++driven_pauses_;
}
void ScriptedThread::wake() { ++wakes_; }

void ScriptedThread::refuse_start() { refusing_ = true; }

std::uint64_t ScriptedThread::wake_handle() const { return wake_handle_; }
std::uint64_t ScriptedThread::woken() const { return woken_; }

void ScriptedThread::await_woken(std::uint64_t seen, std::uint64_t timeout)
{
    ++awaits_;
    last_seen_ = seen;
    last_timeout_ = timeout;
    if (await_hook_ != nullptr) await_hook_(await_context_);
}

void ScriptedThread::offer_wake(std::uint64_t handle) { wake_handle_ = handle; }
void ScriptedThread::driver_woke() { ++woken_; }

void ScriptedThread::on_await(void (*hook)(void *context), void *context)
{
    await_hook_ = hook;
    await_context_ = context;
}

std::uint32_t ScriptedThread::driven_pauses() const { return driven_pauses_; }
std::uint32_t ScriptedThread::awaits() const { return awaits_; }
std::uint64_t ScriptedThread::last_seen() const { return last_seen_; }
std::uint64_t ScriptedThread::last_timeout() const { return last_timeout_; }

}
