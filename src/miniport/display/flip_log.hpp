#ifndef AV_FLIP_LOG_HPP
#define AV_FLIP_LOG_HPP

#include "core/platform/platform.hpp"

#include <stdint.h>

namespace av {

class StateWriter;

class FlipLog {
public:
    static constexpr uint32_t capacity = 512u;

    explicit FlipLog(Platform &platform);
    FlipLog(const FlipLog &) = delete;
    FlipLog &operator=(const FlipLog &) = delete;

    void     note(uint64_t id, uint64_t mark, uint64_t extra);
    uint64_t count();
    void     report(StateWriter &writer, const char *name);

private:
    Platform &platform_;
    uint64_t  next_;
    uint64_t  counters_[capacity];
    uint64_t  ids_[capacity];
    uint64_t  marks_[capacity];
    uint64_t  extras_[capacity];
};

}

#endif
