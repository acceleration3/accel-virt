/* The three shared-object transport commands as a process sends them
 * (feature 053, contracts/wire_additions.md).
 *
 * One class for all three because all three name one object of one kind on one
 * context, and all three ICDs send them: Vulkan publishes what it exported,
 * OpenGL and OpenCL open what another API published. What the device answers
 * is the device's word, so it is read the way any foreign bytes are - a length
 * the contract does not allow is no answer at all, and an empty one is a
 * device older than these commands rather than a refusal.
 *
 * Stateless between calls but for the last status: what a caller turns into
 * its own API's error. */
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

    /* SHARE_OBJECT: `wire_handle`, an object this context's guest already
     * made, published under `object_id` for every context on the device.
     * invalid_parameter for a body the wire refuses - which is not sent - and
     * for a refusal, which device_status() names; malformed for an answer the
     * contract does not allow; not_supported for an empty one; invalid_state
     * when the call itself failed. */
    [[nodiscard]] Status share(uint64_t object_id, uint32_t kind, uint64_t size, uint64_t wire_handle);
    /* OPEN_SHARED_OBJECT: the object opened into this context, answered as the
     * handle the device's executor made of it, which the guest names in its
     * own API's commands from then on. 0 unless ok. */
    [[nodiscard]] Status open(uint64_t object_id, uint32_t kind, uint64_t size, uint64_t &wire_handle);
    /* RELEASE_SHARED_OBJECT: what this context made of the object, destroyed.
     * A refusal is still ok - the object is gone from this context either way,
     * and there is nothing a caller could usefully do about it. */
    [[nodiscard]] Status release(uint64_t object_id, uint32_t kind, uint64_t size);

    /* The status of the device's last answer; 0 after one it took. */
    uint32_t device_status() const;

private:
    /* One of the three, with `handle_allowed` saying whether the answer may
     * carry a wire handle - only an open's may. */
    [[nodiscard]] Status send(uint32_t command_id, uint64_t object_id, uint32_t kind, uint64_t size,
                              uint64_t sent_handle, bool handle_allowed, uint64_t &wire_handle);
    [[nodiscard]] Status judge(WireReader &answer, bool handle_allowed, uint64_t &wire_handle);

    ClientTransport &transport_;
    uint32_t         device_status_;
};

}  // namespace av

#endif  // AV_SHARED_OBJECT_CALL_HPP
