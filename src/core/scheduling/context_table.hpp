/* The adapter's execution contexts. One node, one engine: a context for
 * anything else is refused. */
#ifndef AV_CONTEXT_TABLE_HPP
#define AV_CONTEXT_TABLE_HPP

#include "core/platform/mutex.hpp"
#include "core/platform/platform.hpp"
#include "core/scheduling/context.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class StateWriter;

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
    /* The av context of `follower`'s process whose ring is the allocation
     * `ring` names there, or null: what a FollowRecord on `follower` waits for.
     * A handle names an allocation only in the process that made it, so a
     * process can follow nothing but its own. */
    Context             *followed(const Context &follower, uint32_t ring);
    uint32_t             live() const;

    /* A line for the table and one per context; `busy=true` and nothing more
     * while another thread holds the table. */
    void report(StateWriter &writer);

    bool invariants_hold() const;

private:
    /* SystemContext | GdiContext | VirtualAddressing | HwQueueSupported */
    static constexpr uint32_t known_flags = 0x17u;
    static constexpr uint32_t virtual_addressing = 0x4u;
    static constexpr uint32_t hw_queue_supported = 0x10u;
    /* The nodes this adapter reports, and the one that is hardware scheduled:
     * a context there is one a hardware queue may run on and nothing else
     * (feature 049, research.md §16). */
    static constexpr uint32_t node_count = 2u;
    static constexpr uint32_t hardware_queue_node = 1u;
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
