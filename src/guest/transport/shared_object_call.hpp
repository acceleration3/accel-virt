#ifndef AV_SHARED_OBJECT_CALL_HPP
#define AV_SHARED_OBJECT_CALL_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class ClientTransport;
class WireReader;

class SharedObjectCall {
public:
    explicit SharedObjectCall(ClientTransport &transport);
    SharedObjectCall(const SharedObjectCall &) = delete;
    SharedObjectCall &operator=(const SharedObjectCall &) = delete;

    [[nodiscard]] Status share(uint64_t object_id, uint32_t kind, uint64_t size, uint64_t wire_handle);
    [[nodiscard]] Status open(uint64_t object_id, uint32_t kind, uint64_t size, uint64_t into,
                              uint64_t &wire_handle);
    [[nodiscard]] Status release(uint64_t object_id, uint32_t kind, uint64_t size, uint64_t made);

    uint32_t device_status() const;

private:
    [[nodiscard]] Status send(uint32_t command_id, uint64_t object_id, uint32_t kind, uint64_t size,
                              uint64_t sent_handle, bool handle_allowed, uint64_t &wire_handle);
    [[nodiscard]] Status judge(WireReader &answer, bool handle_allowed, uint64_t &wire_handle);

    ClientTransport &transport_;
    uint32_t         device_status_;
};

}

#endif
