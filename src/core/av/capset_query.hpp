/* Reads the device's av capset (docs/protocol.md): GET_CAPSET_INFO for index
 * 0, then GET_CAPSET for what it names, and judges what comes back against
 * this driver's protocol. One request and one answer at a time: the caller
 * sends each request however it reaches the device and hands back its answer.
 * The first failure ends the query. At most DISPATCH (the DPC's harvest). */
#ifndef AV_CAPSET_QUERY_HPP
#define AV_CAPSET_QUERY_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"
#include "proto/context/capset_info.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct CapsetQueryProbe;

class CapsetQuery {
public:
    explicit CapsetQuery(Platform &platform);

    /* The next request into `bytes`, its size in `length`. invalid_state while
     * an answer is awaited or once the query ended; too_large, writing
     * nothing, when it does not fit. */
    [[nodiscard]] Status next_request(uint8_t *bytes, size_t capacity, size_t &length);
    /* The answer to the request sent. ok moves on. not_supported for a device
     * that refuses, offers another capset or version, or a capset of another
     * major; malformed for an answer no device gives. invalid_state when no
     * answer is awaited. */
    [[nodiscard]] Status accept(const uint8_t *bytes, size_t length);
    bool                 complete() const;
    bool                 failed() const;
    /* Only once complete. */
    const CapsetInfo    &capset() const;

    bool invariants_hold() const;

private:
    enum class Step : uint8_t { send_info, await_info, send_capset, await_capset, done, failed };
    static constexpr uint32_t capset_version = 1u;

    [[nodiscard]] Status accept_info(const uint8_t *bytes, size_t length);
    [[nodiscard]] Status accept_capset(const uint8_t *bytes, size_t length);
    [[nodiscard]] Status fail(Status status, const char *reason);

    Platform  &platform_;
    Step       step_;
    CapsetInfo capset_;
    bool       described_;

    friend struct CapsetQueryProbe;
};

}  // namespace av

#endif  // AV_CAPSET_QUERY_HPP
