/* The driver's root: the DDI table registered with dxgkrnl, and the adapters
 * AddDevice creates. One per driver image. */
#ifndef AV_MINIPORT_HPP
#define AV_MINIPORT_HPP

#include "core/adapter/adapter.hpp"
#include "core/miniport/ddi_table.hpp"
#include "core/platform/platform.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct MiniportProbe;

class Miniport {
public:
    /* One adapter per host GPU; eight is the ceiling. */
    static constexpr uint32_t max_adapters = 8u;

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
    /* The bitwise OR of every table set; the slots are constants, so a
     * non-zero value means the table layout itself is wrong. */
    unsigned               table_status_;
    ObjectStorage<Adapter> adapters_[max_adapters];

    friend struct MiniportProbe;
};

}  // namespace av

#endif  // AV_MINIPORT_HPP
