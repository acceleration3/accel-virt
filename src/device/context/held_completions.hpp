#ifndef AV_HELD_COMPLETIONS_HPP
#define AV_HELD_COMPLETIONS_HPP

#include <stdint.h>

namespace av {

struct HeldCompletionsProbe;

class HeldCompletions {
public:
    static constexpr uint32_t capacity = 128u;

    HeldCompletions();
    HeldCompletions(const HeldCompletions &) = delete;
    HeldCompletions &operator=(const HeldCompletions &) = delete;

    [[nodiscard]] bool hold(uint64_t fence, uint64_t mark);
    uint64_t           release(uint64_t reached);
    uint64_t           release_all();
    bool               empty() const;

    bool invariants_hold() const;

private:
    uint64_t fences_[capacity];
    uint64_t marks_[capacity];
    uint32_t head_;
    uint32_t count_;

    friend struct HeldCompletionsProbe;
};

}

#endif
