#ifndef AV_RUN_SWAP_GATE_HPP
#define AV_RUN_SWAP_GATE_HPP

#include "core/platform/block_source.hpp"
#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

namespace av {

class Host;
class PresentEngine;

class RunSwapGate {
public:
    explicit RunSwapGate(BlockSource &blocks);
    RunSwapGate(const RunSwapGate &) = delete;
    RunSwapGate &operator=(const RunSwapGate &) = delete;

    [[nodiscard]] Status add(PresentEngine &engine);
    void                 remove(PresentEngine &engine);
    [[nodiscard]] bool   begin(Host &host);
    void                 end(Host &host, bool guest_held);
    uint32_t             engines() const;

private:
    using Engines = GrowableArray<PresentEngine *>;

    Engines engines_;
};

}

#endif
