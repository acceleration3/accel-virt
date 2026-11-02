#ifndef AV_SHARED_OBJECT_KIND_HPP
#define AV_SHARED_OBJECT_KIND_HPP

#include <stdint.h>

namespace av {

class SharedObjectKind {
public:
    static constexpr uint32_t memory = 1u;
    static constexpr uint32_t monitored_fence = 2u;
    static constexpr uint32_t native_fence = 3u;
    static constexpr uint64_t page_size = 4096u;

    SharedObjectKind();
    explicit SharedObjectKind(uint32_t value);

    uint32_t value() const;
    bool     known() const;
    bool     is_memory() const;
    bool     is_fence() const;
    bool     same_as(const SharedObjectKind &other) const;
    bool     sized(uint64_t size) const;

private:
    uint32_t value_;
};

}

#endif
