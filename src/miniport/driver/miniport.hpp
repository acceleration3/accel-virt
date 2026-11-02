#ifndef AV_MINIPORT_HPP
#define AV_MINIPORT_HPP

#include "miniport/adapter/adapter.hpp"
#include "miniport/driver/ddi_table.hpp"
#include "core/platform/platform.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "proto/context/av_device_count.hpp"

#include <stdint.h>

namespace av {

struct MiniportProbe;

class Miniport {
public:
    static constexpr uint32_t max_adapters = most_av_devices;

    explicit Miniport(Platform &platform);
    Miniport(const Miniport &) = delete;
    Miniport &operator=(const Miniport &) = delete;

    const DdiTable &ddi_table() const;

    [[nodiscard]] Status add_device(void *physical_device_object, void *&context);
    [[nodiscard]] Status remove_device(void *context);
    void                 unload();

    uint32_t adapter_count() const;
    bool     invariants_hold() const;

private:
    ObjectStorage<Adapter> *slot_of(void *context);

    Platform              &platform_;
    DdiTable               table_;
    unsigned               table_status_;
    ObjectStorage<Adapter> adapters_[max_adapters];

    friend struct MiniportProbe;
};

}

#endif
