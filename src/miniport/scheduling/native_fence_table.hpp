#ifndef AV_NATIVE_FENCE_TABLE_HPP
#define AV_NATIVE_FENCE_TABLE_HPP

#include "core/platform/mutex.hpp"
#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct DxgkargCreateNativeFenceRecord;
struct DxgkargDestroyNativeFenceRecord;
struct DxgkargUpdateNativeFenceValuesRecord;
class StateWriter;

struct NativeFenceTableProbe;

class NativeFenceTable {
public:
    static constexpr uint32_t max_fences = 256u;

    explicit NativeFenceTable(Platform &platform);
    NativeFenceTable(const NativeFenceTable &) = delete;
    NativeFenceTable &operator=(const NativeFenceTable &) = delete;

    [[nodiscard]] Status create(DxgkargCreateNativeFenceRecord &request);
    [[nodiscard]] Status destroy(const DxgkargDestroyNativeFenceRecord &request);
    [[nodiscard]] Status update_monitored(const DxgkargUpdateNativeFenceValuesRecord &request);
    [[nodiscard]] Status update_current(const DxgkargUpdateNativeFenceValuesRecord &request);

    uint32_t live() const;
    uint64_t monitored_value(const void *handle) const;
    uint64_t current_value(const void *handle) const;

    void report(StateWriter &writer);

    bool invariants_hold() const;

private:
    void    *handle_of(uint32_t slot) const;
    uint32_t slot_of(const void *handle) const;
    [[nodiscard]] Status update(const DxgkargUpdateNativeFenceValuesRecord &request, uint64_t (&values)[max_fences]);

    Platform &platform_;
    Mutex     lock_;
    bool      live_slots_[max_fences];
    uint32_t  types_[max_fences];
    uint64_t  monitored_[max_fences];
    uint64_t  current_[max_fences];
    uint32_t  live_;

    friend struct NativeFenceTableProbe;
};

}

#endif
