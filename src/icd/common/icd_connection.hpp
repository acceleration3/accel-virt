#ifndef AV_ICD_CONNECTION_HPP
#define AV_ICD_CONNECTION_HPP

#include "core/support/status.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "icd/common/device_link.hpp"
#include "icd/common/link_generations.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class Kmt;
class StateWriter;
}

namespace av::icd {

class IcdConnection {
public:
    IcdConnection(BlockSource &memory, Kmt &kmt, uint64_t luid);
    IcdConnection(const IcdConnection &) = delete;
    IcdConnection &operator=(const IcdConnection &) = delete;

    [[nodiscard]] Status acquire(DeviceLink *&link);
    void                 release(DeviceLink &link);
    void                 prefer(uint64_t luid);
    uint32_t             adapter() const;
    uint32_t             users() const;
    void                 report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    void close_adapter_if_idle();

    Kmt                         &kmt_;
    uint64_t                     luid_;
    AdapterSearch                search_;
    LinkGenerations<DeviceLink>  links_;

    friend struct IcdConnectionProbe;
};

}

#endif
