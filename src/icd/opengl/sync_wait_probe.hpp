#ifndef AV_GLICD_SYNC_WAIT_PROBE_HPP
#define AV_GLICD_SYNC_WAIT_PROBE_HPP

#include "icd/common/completion_probe.hpp"

#include <stdint.h>

namespace av {
class ClientTransport;
}

namespace av::glicd {

class SyncWaitProbe final : public icd::CompletionProbe {
public:
    enum : uint32_t {
        gl_sync_flush_commands_bit = 0x1u,
        gl_already_signaled = 0x911Au,
        gl_timeout_expired = 0x911Bu,
        gl_condition_satisfied = 0x911Cu,
    };

    SyncWaitProbe(ClientTransport &wire, uint64_t sync, uint32_t flags);

    [[nodiscard]] bool ask(uint64_t slice) override;
    void               pause(uint64_t slice) override;
    uint32_t           result() const;

private:
    ClientTransport &wire_;
    uint64_t         sync_;
    uint32_t         flags_;
    bool             expired_;
    uint32_t         result_;
};

}

#endif
