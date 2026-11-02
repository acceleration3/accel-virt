#ifndef AV_RELEASE_QUEUE_HPP
#define AV_RELEASE_QUEUE_HPP

#include "proto/submission/submission_descriptor.hpp"

#include <stdint.h>

namespace av {

class ReleaseQueue {
public:
    static constexpr uint32_t most_held = 64u;

    enum class Turn : uint8_t { release_now, held, released_already, refused };

    ReleaseQueue();

    Turn     arrive(const SubmissionDescriptor &batch, uint64_t tail);
    void     released(const SubmissionDescriptor &batch);
    bool     next_held(SubmissionDescriptor &batch);
    bool     active() const;
    void     clear();
    uint32_t held() const;

private:
    SubmissionDescriptor held_[most_held];
    uint32_t             count_;
    uint64_t             next_;
    bool                 active_;
};

}

#endif
