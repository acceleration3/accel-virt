#ifndef AV_RECORDING_THREAD_LOCK_HPP
#define AV_RECORDING_THREAD_LOCK_HPP

#include "core/platform/thread_lock.hpp"

#include <cstdint>

namespace av {

class LockTrace;

class RecordingThreadLock final : public ThreadLock {
public:
    RecordingThreadLock(LockTrace &trace, char letter);
    ~RecordingThreadLock();

    void acquire() override;
    bool try_acquire() override;
    void release() override;
    void held_elsewhere(bool held);

    bool          held() const;
    std::uint32_t acquisitions() const;
    bool          claim_violation();

private:
    static constexpr char lower_case = 0x20;

    LockTrace    &trace_;
    char          letter_;
    bool          held_;
    bool          elsewhere_;
    std::uint32_t acquisitions_;
    std::uint32_t violations_;
};

}

#endif
