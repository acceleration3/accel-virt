#ifndef AV_INSPECT_HOST_HPP
#define AV_INSPECT_HOST_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class InspectHost {
public:
    InspectHost() = default;
    InspectHost(const InspectHost &) = delete;
    InspectHost &operator=(const InspectHost &) = delete;

    virtual uint32_t processes(uint32_t *pids, uint32_t capacity) = 0;
    virtual Status   executable(uint32_t pid, char *path, size_t capacity) = 0;
    virtual Status   copy_section(const char *name, uint8_t *destination, size_t capacity, size_t &copied) = 0;
    virtual Status   request_state(const char *dump, const char *done, uint32_t milliseconds) = 0;
    virtual Status   write_file(const char *path, const uint8_t *head, size_t head_bytes, const uint8_t *body,
                                size_t body_bytes) = 0;
    virtual uint64_t clock() const = 0;

protected:
    ~InspectHost() = default;
};

}

#endif
