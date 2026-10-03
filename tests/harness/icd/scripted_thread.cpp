#include "harness/icd/scripted_thread.hpp"

namespace av {

ScriptedThread::ScriptedThread()
    : loop_(nullptr), turns_(0u), starts_(0u), stops_(0u), rests_(0u), wakes_(0u), stopping_(false), refusing_(false)
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
std::uint32_t ScriptedThread::wakes() const { return wakes_; }
void ScriptedThread::rest() { ++rests_; }
void ScriptedThread::wake() { ++wakes_; }

void ScriptedThread::refuse_start() { refusing_ = true; }

}  // namespace av
