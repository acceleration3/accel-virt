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

    [[nodiscard]] Status next_request(uint8_t *bytes, size_t capacity, size_t &length);
    [[nodiscard]] Status accept(const uint8_t *bytes, size_t length);
    bool                 complete() const;
    bool                 failed() const;
    const CapsetInfo    &capset() const;

    bool invariants_hold() const;

private:
    enum class Step : uint8_t { send_info, await_info, send_capset, await_capset, done, failed };
    static constexpr uint32_t capset_version = 2u;

    [[nodiscard]] Status accept_info(const uint8_t *bytes, size_t length);
    [[nodiscard]] Status accept_capset(const uint8_t *bytes, size_t length);
    [[nodiscard]] Status fail(Status status, const char *reason);

    Platform  &platform_;
    Step       step_;
    CapsetInfo capset_;
    bool       described_;

    friend struct CapsetQueryProbe;
};

}

#endif
