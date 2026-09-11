/* A device's contexts, by the ids the guest gives them. Context 0 is
 * virtio-gpu's "no context" and is never created. Destroying a context stops
 * and joins its worker; reset destroys every context. Main loop only. */
#ifndef AV_DEVICE_CONTEXT_TABLE_HPP
#define AV_DEVICE_CONTEXT_TABLE_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "device/context/device_context.hpp"

#include <stdint.h>

namespace av {

class Host;
struct DeviceContextTableProbe;

class DeviceContextTable {
public:
    /* One worker thread each, so a bound on threads as much as on contexts. */
    static constexpr uint32_t max_contexts = 64u;

    explicit DeviceContextTable(Host &host);
    ~DeviceContextTable();
    DeviceContextTable(const DeviceContextTable &) = delete;
    DeviceContextTable &operator=(const DeviceContextTable &) = delete;

    /* invalid_parameter for id 0, conflict for a live id, no_space when full,
     * or the worker's failure, leaving no context behind. */
    [[nodiscard]] Status create(uint32_t id);
    [[nodiscard]] Status destroy(uint32_t id);
    void                 destroy_all();
    DeviceContext       *find(uint32_t id);
    uint32_t             live() const;

    bool invariants_hold() const;

private:
    ObjectStorage<DeviceContext> *slot_of(uint32_t id);

    Host                        &host_;
    ObjectStorage<DeviceContext> slots_[max_contexts];

    friend struct DeviceContextTableProbe;
};

}  // namespace av

#endif  // AV_DEVICE_CONTEXT_TABLE_HPP
