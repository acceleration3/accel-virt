/* What one feasibility probe found on this host: whether it ran, whether what
 * it asked for worked, and the host's own number for the refusal. A capability
 * bit whose evidence changed underneath it is worse than no bit, so a result is
 * answered once and then only read. */
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

    /* `measured` is what the probe carried: bytes, or a rate. invalid_state
     * when this result was already answered. */
    [[nodiscard]] Status record_pass(uint64_t measured);
    /* `host_error` is the host's own number - a VkResult or an errno - and 0
     * when the host had none to give. */
    [[nodiscard]] Status record_failure(Status reason, int32_t host_error);

    /* What a failed probe measured about the path taken instead; ignored once
     * the result passed, whose measurement is the probe's own. */
    void     measure(uint64_t measured);

    bool     ran() const;
    bool     passed() const;
    Status   reason() const;
    int32_t  errno_value() const;
    uint64_t measured() const;
    /* The one line the device logs for this probe. */
    void     describe(TextBuffer &line) const;

private:
    enum class Outcome : uint8_t { not_run, passed, failed };

    Outcome  outcome_;
    Status   reason_;
    int32_t  errno_;
    uint64_t measured_;
};

}  // namespace av

#endif  // AV_PROBE_RESULT_HPP
