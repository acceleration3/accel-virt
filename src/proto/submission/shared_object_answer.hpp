#ifndef AV_SHARED_OBJECT_ANSWER_HPP
#define AV_SHARED_OBJECT_ANSWER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class SharedObjectAnswer {
public:
    static constexpr size_t encoded_size = 16u;

    enum : uint32_t {
        done = 0u,
        refused_request = 1u,
        unknown_object = 2u,
        mismatched_object = 3u,
        executor_refused = 4u,
        no_room = 5u,
        conflict = 6u,
    };

    SharedObjectAnswer();
    SharedObjectAnswer(uint32_t status, uint64_t wire_handle);

    uint32_t status() const;
    uint64_t wire_handle() const;
    bool     succeeded() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    bool fields_valid() const;

    uint32_t status_;
    uint64_t wire_handle_;
};

}

#endif
