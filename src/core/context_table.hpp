/* The adapter's execution contexts. One node, one engine: a context for
 * anything else, or with GPU virtual addressing, is refused. */
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
struct ContextTableProbe;
struct DxgkargCreateContextRecord;

class ContextTable {
public:
    ContextTable(Platform &platform, DeviceTable &devices);
    ContextTable(const ContextTable &) = delete;
    ContextTable &operator=(const ContextTable &) = delete;

    [[nodiscard]] Status create(void *device, DxgkargCreateContextRecord &request);
    [[nodiscard]] Status destroy(void *context);
    Context             *find(void *context);
    uint32_t             live() const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t known_flags = 0x3u;       /* SystemContext | GdiContext */
    static constexpr uint32_t known_engines = 0x1u;

    Platform            &platform_;
    DeviceTable         &devices_;
    Mutex                mutex_;
    ObjectPool<Context>  contexts_;

    friend struct ContextTableProbe;
};

}  // namespace av

#endif  // AV_CONTEXT_TABLE_HPP
