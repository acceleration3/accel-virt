/* The adapter's execution contexts. One node, one engine: a context for
 * anything else is refused. */
#ifndef AV_CONTEXT_TABLE_HPP
#define AV_CONTEXT_TABLE_HPP

#include "context.hpp"
#include "mutex.hpp"
#include "object_pool.hpp"
#include "platform.hpp"
#include "status.hpp"

#include <stdint.h>

namespace av {

class DeviceTable;
class GpuMmuGeometry;
struct ContextTableProbe;
struct DxgkargCreateContextRecord;

class ContextTable {
public:
    ContextTable(Platform &platform, DeviceTable &devices, const GpuMmuGeometry &geometry);
    ContextTable(const ContextTable &) = delete;
    ContextTable &operator=(const ContextTable &) = delete;

    [[nodiscard]] Status create(void *device, DxgkargCreateContextRecord &request);
    [[nodiscard]] Status destroy(void *context);
    Context             *find(void *context);
    uint32_t             live() const;

    bool invariants_hold() const;

private:
    /* SystemContext | GdiContext | VirtualAddressing | HwQueueSupported */
    static constexpr uint32_t known_flags = 0x17u;
    static constexpr uint32_t virtual_addressing = 0x4u;
    static constexpr uint32_t hw_queue_supported = 0x10u;
    static constexpr uint32_t known_engines = 0x1u;

    Platform            &platform_;
    DeviceTable         &devices_;
    const GpuMmuGeometry &geometry_;
    Mutex                mutex_;
    ObjectPool<Context>  contexts_;

    friend struct ContextTableProbe;
};

}  // namespace av

#endif  // AV_CONTEXT_TABLE_HPP
