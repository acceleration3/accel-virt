/* glClientWaitSync asked of the host a slice at a time (HostCompletionWait),
 * where it was once forwarded whole - GL_TIMEOUT_IGNORED included - and held
 * the context's host worker for as long as the sync took. The host blocks for
 * the slice itself, so nothing pauses between asks.
 *
 * What the application sees is one wait's answer: GL_SYNC_FLUSH_COMMANDS_BIT
 * goes with the first slice only, as one wait flushes once, and a sync found
 * signalled after a slice expired was waited for - GL_CONDITION_SATISFIED,
 * not GL_ALREADY_SIGNALED. */
#ifndef AV_GLICD_SYNC_WAIT_PROBE_HPP
#define AV_GLICD_SYNC_WAIT_PROBE_HPP

#include "icd/common/completion_probe.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
}  // namespace av

namespace av::glicd {

class SyncWaitProbe final : public icd::CompletionProbe {
public:
    enum : uint32_t {
        gl_sync_flush_commands_bit = 0x1u,
        gl_already_signaled = 0x911Au,
        gl_timeout_expired = 0x911Bu,
        gl_condition_satisfied = 0x911Cu,
    };

    /* The host's sync `sync`, waited for with `flags`. */
    SyncWaitProbe(ClientTransport &wire, uint64_t sync, uint32_t flags);

    [[nodiscard]] bool ask(uint64_t slice) override;
    void               pause(uint64_t slice) override;
    /* The wait's answer: GL_TIMEOUT_EXPIRED until a slice ends it. */
    uint32_t           result() const;

private:
    ClientTransport &wire_;
    uint64_t         sync_;
    uint32_t         flags_;
    bool             expired_;
    uint32_t         result_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_SYNC_WAIT_PROBE_HPP
