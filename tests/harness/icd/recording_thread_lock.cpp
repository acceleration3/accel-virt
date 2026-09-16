#include "harness/icd/recording_thread_lock.hpp"

#include "harness/framework/test_registry.hpp"
#include "harness/icd/lock_trace.hpp"

namespace av {

RecordingThreadLock::RecordingThreadLock(LockTrace &trace, char letter)
    : trace_(trace), letter_(letter), held_(false), elsewhere_(false), acquisitions_(0u), violations_(0u) {}

RecordingThreadLock::~RecordingThreadLock()
{
    TestRegistry &registry = TestRegistry::instance();
    if (violations_ != 0u)
        registry.record_failure(__FILE__, __LINE__, "lock %c: %u unclaimed violation(s)", letter_, violations_);
    if (held_) registry.record_failure(__FILE__, __LINE__, "lock %c still held", letter_);
}

void RecordingThreadLock::acquire()
{
    trace_.append(letter_);
    if (held_) {
        violations_++;
        return;
    }
    held_ = true;
    acquisitions_++;
}

bool RecordingThreadLock::try_acquire()
{
    if (held_ || elsewhere_) {
        trace_.append('!');
        return false;
    }
    acquire();
    return true;
}

void RecordingThreadLock::held_elsewhere(bool held) { elsewhere_ = held; }

void RecordingThreadLock::release()
{
    trace_.append(static_cast<char>(letter_ | lower_case));
    if (!held_) {
        violations_++;
        return;
    }
    held_ = false;
}

bool          RecordingThreadLock::held() const { return held_; }
std::uint32_t RecordingThreadLock::acquisitions() const { return acquisitions_; }

bool RecordingThreadLock::claim_violation()
{
    if (violations_ == 0u) return false;
    violations_--;
    return true;
}

}  // namespace av
