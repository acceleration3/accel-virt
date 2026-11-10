#ifndef AV_CLICD_CL_OBJECT_TABLE_HPP
#define AV_CLICD_CL_OBJECT_TABLE_HPP

#include "icd/common/handle_tree.hpp"
#include "icd/opencl/cl_object.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class StateWriter;
class ThreadLock;
}

namespace av::clicd {

struct ClObjectTableProbe;

class ClObjectTable {
public:
    ClObjectTable(BlockSource &memory, const cl_icd_dispatch &table, ThreadLock &lock);
    ClObjectTable(const ClObjectTable &) = delete;
    ClObjectTable &operator=(const ClObjectTable &) = delete;

    [[nodiscard]] Status make(ClObject::Kind kind, uint64_t wire, GuestClContext *context,
                              ClObject *parent, ClObject *&object);

    ClObject *made(const void *pointer, ClObject::Kind kind) const;

    bool release(ClObject &object, GuestClContext *&freed);
    bool release(ClObject &object, GuestClContext *&freed, const void **gone, uint32_t room, uint32_t &count);
    void destroy(ClObject &object);

    uint32_t live() const;
    uint32_t live_of(ClObject::Kind kind) const;
    bool     queues_idle(const ClObject *context) const;
    uint32_t poison_gated_queues(const ClObject *context);
    void     report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    friend struct ClObjectTableProbe;

    void destroy_held(ClObject &object);

    BlockSource              &memory_;
    icd::HandleTree<ClObject> tree_;
    const cl_icd_dispatch    &table_;
    ThreadLock               &lock_;
};

}

#endif
