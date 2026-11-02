#ifndef AV_SHARED_OBJECT_TABLE_HPP
#define AV_SHARED_OBJECT_TABLE_HPP

#include "miniport/memory/shared_object.hpp"
#include "core/platform/mutex.hpp"
#include "core/platform/platform.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class Allocation;
class SharedObjectRecord;
class StateWriter;
struct SharedObjectTableProbe;

class SharedObjectTable {
public:
    explicit SharedObjectTable(Platform &platform);
    SharedObjectTable(const SharedObjectTable &) = delete;
    SharedObjectTable &operator=(const SharedObjectTable &) = delete;

    [[nodiscard]] Status initialize();
    uint64_t             adapter() const;

    [[nodiscard]] Status mint(Allocation &allocation, SharedObjectRecord &record);
    [[nodiscard]] Status open(const SharedObjectRecord &record, SharedObject *&found);
    [[nodiscard]] Status open_minted(uint64_t object_id, SharedObject *&found, SharedObjectRecord &minted);
    [[nodiscard]] Status close(SharedObject &object);
    void                 forget(const Allocation &allocation);
    uint32_t             take_released(uint64_t *ids, uint32_t capacity);
    uint32_t             live() const;

    void report(StateWriter &writer);

    bool invariants_hold() const;

private:
    SharedObject *find(uint64_t id);

    Platform                 &platform_;
    Mutex                     mutex_;
    ObjectPool<SharedObject>  objects_;
    uint64_t                  next_id_;
    uint64_t                  adapter_;

    friend struct SharedObjectTableProbe;
};

}

#endif
