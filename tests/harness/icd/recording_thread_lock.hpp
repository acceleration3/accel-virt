/* The ICD's ThreadLock in a test: one thread, so a lock is either held or not,
 * and every acquire and release lands in a shared LockTrace under this lock's
 * letter. Acquiring a lock already held - a deadlock on a real SRWLOCK - and
 * releasing one not held are violations; an unclaimed violation, or a lock left
 * held, fails the test when this is destroyed, as MockPlatform's rules do. */
#ifndef AV_RECORDING_THREAD_LOCK_HPP
#define AV_RECORDING_THREAD_LOCK_HPP

#include "icd/vulkan/thread_lock.hpp"

#include <cstdint>

namespace av {

class LockTrace;

class RecordingThreadLock final : public icd::ThreadLock {
public:
    /* `letter` is upper case; its release is recorded in lower case. */
    RecordingThreadLock(LockTrace &trace, char letter);
    ~RecordingThreadLock();

    void acquire() override;
    /* Recorded as the letter when taken and `!` when refused: refused while
     * held here or while the test says another thread holds it. */
    bool try_acquire() override;
    void release() override;
    void held_elsewhere(bool held);

    bool          held() const;
    std::uint32_t acquisitions() const;
    /* Removes one recorded violation; false when there was none. */
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

}  // namespace av

#endif  // AV_RECORDING_THREAD_LOCK_HPP
