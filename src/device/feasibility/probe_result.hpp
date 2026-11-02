#ifndef AV_PROBE_RESULT_HPP
#define AV_PROBE_RESULT_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class TextBuffer;

class ProbeResult {
public:
    ProbeResult();
    ProbeResult(const ProbeResult &) = delete;
    ProbeResult &operator=(const ProbeResult &) = delete;

    [[nodiscard]] Status record_pass(uint64_t measured);
    [[nodiscard]] Status record_failure(Status reason, int32_t host_error);

    void     measure(uint64_t measured);

    bool     ran() const;
    bool     passed() const;
    Status   reason() const;
    int32_t  errno_value() const;
    uint64_t measured() const;
    void     describe(TextBuffer &line) const;

private:
    enum class Outcome : uint8_t { not_run, passed, failed };

    Outcome  outcome_;
    Status   reason_;
    int32_t  errno_;
    uint64_t measured_;
};

}

#endif
