#ifndef AV_AV_CONTEXT_TABLE_HPP
#define AV_AV_CONTEXT_TABLE_HPP

#include "core/platform/platform.hpp"
#include "core/platform/spin_lock.hpp"
#include "core/platform/wait_mutex.hpp"
#include "miniport/av_context/chunk_ids.hpp"
#include "miniport/av_context/context_chunk.hpp"
#include "miniport/av_context/context_memory.hpp"
#include "miniport/av_context/release_queue.hpp"
#include "miniport/av_context/ring_submitter.hpp"
#include "miniport/av_context/av_context_binding.hpp"
#include "miniport/av_context/submission_reader.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "proto/resources/allocation_kind.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class StateWriter;

class Allocation;
class AllocationTable;
class CapsetInfo;
class ContextViewsRecord;
class DxgkrnlInterface;
class ExecutionNode;
class HwQueueSubmissions;
class GuestPageList;
class HostWindow;
class HostWindowRecord;
class RegisterWindow;
class SubmissionDescriptor;
class AvContextRecord;
class VirtioGpuDevice;
struct AvContextTableProbe;

class AvContextTable {
public:
    static constexpr uint32_t max_contexts = RingSubmitter::doorbell_pages;
    static constexpr uint64_t max_adopted_bytes = uint64_t{1} << 30u;

    AvContextTable(Platform &platform, DxgkrnlInterface &dxgkrnl, AllocationTable &allocations,
                    HostWindow &window);
    ~AvContextTable();
    AvContextTable(const AvContextTable &) = delete;
    AvContextTable &operator=(const AvContextTable &) = delete;

    [[nodiscard]] Status bind(void *context, const AvContextRecord &record, VirtioGpuDevice &device,
                              RegisterWindow &doorbells, const CapsetInfo &capset);
    [[nodiscard]] Status unbind(void *context, VirtioGpuDevice *device);
    [[nodiscard]] Status views(void *context, ContextViewsRecord &record);
    [[nodiscard]] Status wake_on_raise(void *context, uint64_t handle);
    [[nodiscard]] Status release_shared(uint64_t object_id, VirtioGpuDevice &device);
    [[nodiscard]] Status submit(void *context, const uint8_t *private_data, size_t size, uint64_t &awaited);
    [[nodiscard]] Status release_batch(void *context, const SubmissionDescriptor &batch, uint64_t &awaited);
    void                 serve(ExecutionNode &node, HwQueueSubmissions &queues);
    bool                 fault_if_held(void *context);
    bool                 completed_already(void *context, uint64_t fence);
    void                 kick(void *context);
    void                 abandon_all();
    void                *user_of(const Allocation *allocation);
    [[nodiscard]] Status window_chunk(void *context, HostWindowRecord &record, VirtioGpuDevice &device);
    uint32_t             bound() const;
    bool                 holds(const void *context) const;
    uint32_t             doorbell_of(const void *context) const;
    [[nodiscard]] Status user_mode_submits(const void *context, uint64_t *progress);
    [[nodiscard]] Status driver_submits(const void *context);
    bool                 forget_progress(const void *context, const uint64_t *progress);

    void report(StateWriter &writer);

    bool invariants_hold() const;

private:
    static constexpr uint32_t no_slot = max_contexts;

    uint32_t             slot_of(const void *context) const;
    bool                 lost(uint32_t slot) const;
    void                 wake(uint32_t slot, uint64_t raised, bool lost_now);
    uint32_t             claim(void *context, bool &duplicate);
    [[nodiscard]] Status resolve(uint32_t handle, AllocationKind kind, Allocation *&allocation);
    [[nodiscard]] Status resolve_share(uint32_t slot, SubApi asking, uint32_t handle, uint32_t &context_id);
    [[nodiscard]] Status set_up(uint32_t slot, const AvContextRecord &record, VirtioGpuDevice &device,
                                RegisterWindow &doorbells, const CapsetInfo &capset);
    void                 release(uint32_t slot);
    [[nodiscard]] Status in_turn(uint32_t slot, const SubmissionDescriptor &batch);
    [[nodiscard]] Status lose(uint32_t slot);
    void                 release_chunks(uint32_t slot, VirtioGpuDevice *device, bool host_let_go);
    [[nodiscard]] Status open_window(uint32_t slot, HostWindowRecord &record, VirtioGpuDevice &device);
    [[nodiscard]] Status adopt_pages(uint32_t slot, HostWindowRecord &record, VirtioGpuDevice &device);
    [[nodiscard]] Status adopt_locked(uint32_t slot, void *locked, HostWindowRecord &record, VirtioGpuDevice &device);
    [[nodiscard]] Status place_in_window(uint32_t slot, uint32_t resource, HostWindowRecord &record,
                                         VirtioGpuDevice &device);
    [[nodiscard]] Status close_window(uint32_t slot, uint32_t resource_id, VirtioGpuDevice &device);
    ContextChunk        *find_chunk(uint32_t slot, uint32_t resource_id);
    void                 take_down(ContextChunk &chunk, VirtioGpuDevice *device, bool host_let_go);
    void                 log_binding(uint32_t slot, uint32_t message, Status status);

    Platform                        &platform_;
    DxgkrnlInterface                &dxgkrnl_;
    AllocationTable                 &allocations_;
    HostWindow                      &window_;
    SpinLock                         lock_;
    SubmissionReader                 reader_;
    ObjectStorage<AvContextBinding> bindings_[max_contexts];
    ReleaseQueue                     releases_[max_contexts];
    void                            *contexts_[max_contexts];
    const Allocation                *names_[max_contexts];
    ObjectStorage<ContextMemory>     memories_[max_contexts];
    ObjectPool<ContextChunk>         chunks_;
    ChunkIds                         ids_;
    uint32_t                         held_chunks_[max_contexts];
    uint8_t                          orders_[max_contexts];
    void                            *wakes_[max_contexts];
    uint64_t                         raised_[max_contexts];
    bool                             told_lost_[max_contexts];
    uint32_t                         generations_[max_contexts];
    bool                             live_[max_contexts];
    uint8_t                          request_[AvContextBinding::request_bytes];
    WaitMutex                        requests_;

    friend struct AvContextTableProbe;
};

}

#endif
