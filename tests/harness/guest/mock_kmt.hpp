/* A D3DKMT that answers what a test scripts: adapters with their identity and
 * driver version, handles handed out in sequence, any call failed with an
 * NTSTATUS or answered with a zero handle, and a count of every object still
 * open and every call made with a handle it never gave. Allocations decode
 * their descriptor as the driver does; submissions must land inside a
 * mapping of the submitting context's device; a lock hands out the one view
 * this mock owns, which keeps what is written through it; and a wait on a
 * transport's context can find a scripted answer in its reply slot - one for
 * every wait, or a queue of them, one per wait - with the ring consumed up to
 * the last submission, as a device that finished it would. Every command a
 * transport's submission carried is journalled by id and size. An escape binds
 * and unbinds host-visible chunks as the driver does, and a test can run its
 * own callback at a chosen wait to play the host's work on the views. */
#ifndef AV_MOCK_KMT_HPP
#define AV_MOCK_KMT_HPP

#include "guest/kmt/kmt.hpp"
#include "guest/kmt/pci_identity.hpp"

#include <cstdint>

namespace av {

/* The driver's side of an adapter-only escape, as a test plays it. */
class AdapterEscapes {
public:
    AdapterEscapes() = default;
    AdapterEscapes(const AdapterEscapes &) = delete;
    AdapterEscapes &operator=(const AdapterEscapes &) = delete;
    /* The NTSTATUS the escape answers; `data` may be written. */
    virtual std::uint32_t answer(std::uint8_t *data, std::uint32_t size) = 0;

protected:
    ~AdapterEscapes() = default;
};

class MockKmt final : public Kmt {
public:
    enum class Call : unsigned {
        enumerate_adapters,
        query_identity,
        query_driver_version,
        close_adapter,
        create_device,
        destroy_device,
        create_allocation,
        destroy_allocation,
        share_object,
        open_shared_object,
        close_shared_object,
        lock,
        unlock,
        create_paging_queue,
        destroy_paging_queue,
        make_resident,
        map_gpu_virtual_address,
        free_gpu_virtual_address,
        wait_for_fence,
        create_context,
        create_context_virtual,
        create_hw_queue_context,
        destroy_context,
        submit,
        wait_for_context,
        create_hw_queue,
        submit_to_hw_queue,
        wait_for_hw_queue,
        destroy_hw_queue,
        create_doorbell,
        connect_doorbell,
        destroy_doorbell,
        notify_work_submission,
        wait_for_vertical_blank,
        escape,
        adapter_luid,
        query_video_memory,
        count
    };
    static constexpr std::uint32_t max_objects = 32u;
    static constexpr std::uint32_t invalid_parameter = 0xC000000Du;
    static constexpr std::uint32_t pending = 0x00000103u;
    static constexpr std::uint64_t page_bytes = 4096u;
    /* The performance counter this answers, in the 100ns units Windows uses. */
    static constexpr std::uint64_t ticks_per_second = 10000000u;
    /* The memory each lock of an allocation up to this size addresses; several
     * may be held at once. A larger allocation's lock addresses all of it. */
    static constexpr std::uint32_t view_capacity = 8192u;
    static constexpr std::uint32_t max_locks = 16u;
    /* The largest context or escape private data this records for a test to
     * read back. */
    static constexpr std::uint32_t max_private = 64u;
    /* The largest allocation private driver data this keeps, which is a
     * descriptor and a shared object's record with room over (feature 053). */
    static constexpr std::uint32_t max_allocation_private = 128u;
    /* NT handles this hands out at once: one more than icd::SharedHandles
     * holds, so a test can reach the ICD table's own refusal and not only the
     * kernel's (feature 053). */
    static constexpr std::uint32_t max_shared = 65u;
    /* The first NT handle it mints, which is nothing a D3DKMT_HANDLE could be
     * so a test that confuses the two fails rather than passing by luck. */
    static constexpr std::uint64_t first_shared_handle = 0x8000000000000004ull;
    /* The largest answer a test scripts for a transport's context: room for a
     * Vulkan device's memory properties and their framing. */
    static constexpr std::uint32_t max_answer = 1024u;
    /* Host-visible chunks bound at once, and the resource the first bind
     * answers: a context's first chunk slot after its three transport
     * resources. */
    static constexpr std::uint32_t max_bindings = 8u;
    static constexpr std::uint32_t first_chunk_resource = 0x00100003u;
    static constexpr std::uint32_t not_supported = 0xC00000BBu;
    /* STATUS_NOT_FOUND: what a handle this mock never minted answers. */
    static constexpr std::uint32_t not_found = 0xC0000225u;
    /* The LUID the Nth adapter added answers until a test sets another:
     * first_luid + N. */
    static constexpr std::uint64_t first_luid = 0x0000000100000000ull;
    static constexpr std::uint64_t default_local_budget = std::uint64_t{256} << 20u;
    static constexpr std::uint64_t default_non_local_budget = std::uint64_t{1} << 30u;
    /* Answers a test can queue, and commands the journal keeps. */
    static constexpr std::uint32_t max_queued = 320u;
    static constexpr std::uint32_t max_journal = 128u;
    /* The leading bytes of each journalled command a test can read back. */
    static constexpr std::uint32_t journal_bytes = 128u;

    MockKmt();
    ~MockKmt();
    MockKmt(const MockKmt &) = delete;
    MockKmt &operator=(const MockKmt &) = delete;

    Status        enumerate_adapters(std::uint32_t *adapters, std::uint32_t capacity, std::uint32_t &count) override;
    Status        query_identity(std::uint32_t adapter, PciIdentity &identity) override;
    Status        query_driver_version(std::uint32_t adapter, std::uint32_t &version) override;
    Status        close_adapter(std::uint32_t adapter) override;
    Status        adapter_luid(std::uint32_t adapter, std::uint64_t &luid) override;
    Status        query_video_memory(std::uint32_t adapter, MemorySegmentGroup group, std::uint64_t &budget,
                                     std::uint64_t &usage) override;
    Status        create_device(std::uint32_t adapter, std::uint32_t &device) override;
    Status        destroy_device(std::uint32_t device) override;
    Status        create_allocation(std::uint32_t device, const std::uint8_t *descriptor, std::uint32_t size,
                                    std::uint32_t &allocation) override;
    Status        destroy_allocation(std::uint32_t device, std::uint32_t allocation) override;
    Status        share_object(std::uint32_t device, std::uint32_t allocation, std::uint64_t &handle) override;
    Status        open_shared_object(std::uint32_t device, std::uint64_t handle, std::uint32_t &allocation,
                                     std::uint8_t *private_data, std::uint32_t capacity,
                                     std::uint32_t &written) override;
    Status        close_shared_object(std::uint64_t handle) override;
    Status        lock(std::uint32_t device, std::uint32_t allocation, void *&view) override;
    Status        unlock(std::uint32_t device, std::uint32_t allocation) override;
    Status        create_paging_queue(std::uint32_t device, std::uint32_t &queue, std::uint32_t &fence_object) override;
    Status        destroy_paging_queue(std::uint32_t queue) override;
    Status        make_resident(std::uint32_t queue, std::uint32_t allocation, std::uint64_t &paging_fence) override;
    Status        map_gpu_virtual_address(std::uint32_t queue, std::uint32_t allocation, std::uint64_t pages,
                                          std::uint64_t &address, std::uint64_t &paging_fence) override;
    Status        free_gpu_virtual_address(std::uint32_t adapter, std::uint64_t address, std::uint64_t pages) override;
    Status        wait_for_fence(std::uint32_t device, std::uint32_t fence_object, std::uint64_t fence) override;
    Status        create_context(std::uint32_t device, std::uint32_t &context) override;
    Status        create_hw_queue_context(std::uint32_t device, const std::uint8_t *private_data,
                                          std::uint32_t size, std::uint32_t &context) override;
    Status        create_context_virtual(std::uint32_t device, const std::uint8_t *private_data, std::uint32_t size,
                                        std::uint32_t &context) override;
    Status        destroy_context(std::uint32_t context) override;
    Status        submit(std::uint32_t context, std::uint64_t commands, std::uint64_t length,
                         const std::uint8_t *private_data, std::uint32_t private_size) override;
    Status        wait_for_context(std::uint32_t device, std::uint32_t context) override;
    /* Feature 049. A hardware queue is answered only while
     * `hardware_scheduling_` is on, which a test turns off to play an OS that
     * will not give this adapter one; a doorbell answers three pages of this
     * mock's own memory, which a test reads to see what the transport stored.
     */
    Status        create_hw_queue(std::uint32_t device, std::uint32_t context, GuestHwQueue &queue) override;
    Status        submit_to_hw_queue(std::uint32_t queue, std::uint64_t commands, std::uint64_t length,
                                     const std::uint8_t *private_data, std::uint32_t private_size,
                                     std::uint64_t progress_fence_id) override;
    Status        wait_for_hw_queue(std::uint32_t device, std::uint32_t progress_fence,
                                    std::uint64_t value) override;
    Status        destroy_hw_queue(std::uint32_t queue) override;
    Status        create_doorbell(std::uint32_t queue, std::uint32_t ring, std::uint32_t &doorbell, void *&page,
                                  const std::uint32_t *&status, std::uint64_t *&queued) override;
    Status        connect_doorbell(std::uint32_t doorbell) override;
    Status        destroy_doorbell(std::uint32_t doorbell) override;
    Status        notify_work_submission(std::uint32_t doorbell) override;

    /* What a test plays with: whether the OS gives this adapter a hardware
     * queue at all, what a process reads out of a doorbell's status page, and
     * the stores it made to the doorbell. */
    /* Feature 049: whether this OS gives the adapter hardware queues at all.
     * Off by default, as Windows is until hardware-accelerated GPU scheduling
     * is turned on (research.md §11), so a test that wants the doorbell path
     * asks for it. */
    void          allow_hardware_scheduling();
    void          refuse_hardware_scheduling();
    void          set_doorbell_status(std::uint32_t doorbell, std::uint32_t status);
    /* A driver that answers a doorbell and leaves out one of the three
     * addresses it is supposed to map - the page, the status or the queued
     * fence, numbered from 1 - which is what a process must not believe
     * (feature 049). */
    void          answer_doorbell_without_pages(std::uint32_t which);
    /* What is in the doorbell's page now: a process's store is a four-byte
     * write of a value that carries nothing, so what a test reads is whether
     * it was stored to since the page was last cleared. */
    std::uint32_t doorbell_page(std::uint32_t doorbell) const;
    void          clear_doorbell_page(std::uint32_t doorbell);
    std::uint64_t queued_fence(std::uint32_t doorbell) const;
    /* The handle of the `index`th doorbell ever made here, 0 past the end: how
     * a test reaches the doorbell a transport made for itself. */
    std::uint32_t doorbell_handle(std::uint32_t index) const;
    Status        wait_for_vertical_blank(std::uint32_t adapter, std::uint32_t device, std::uint32_t source) override;
    /* As the driver answers a HostVisibleBindRecord: a bind of a host-visible
     * allocation of `device` answers first_chunk_resource + the binds so far
     * and the allocation's pages; an unbind names a resource still bound;
     * either on a lost context is refused. Destroying a bound allocation
     * unbinds it, as the driver does when it tears the context down. */
    Status        escape(std::uint32_t adapter, std::uint32_t device, std::uint32_t context, std::uint8_t *data,
                         std::uint32_t size) override;
    std::uint64_t performance_counter(std::uint64_t &frequency) const override;
    std::uint32_t last_code() const override;

    /* An adapter enumeration opens; up to eight. */
    void          add_adapter(const PciIdentity &identity, std::uint32_t driver_version);
    /* The LUID of the adapter added `index`th (0-based); past the adapters
     * kept, nothing. */
    void          set_adapter_luid(std::uint32_t index, std::uint64_t luid);
    /* What a video memory query of `group` answers for every adapter; a group
     * D3DKMT does not define is not kept. */
    void          set_video_memory(MemorySegmentGroup group, std::uint64_t budget, std::uint64_t usage);
    /* The next enumeration claims this many adapters, whatever the capacity. */
    void          claim_adapters(std::uint32_t count);
    /* The Nth `call` from now (1-based) answers `code`; a success code answers success. */
    void          fail_at(Call call, std::uint32_t nth, std::uint32_t code);
    /* The first `count` calls fail and the ones after them do not: what a host
     * refusing an ask a driver then repeats differently looks like (feature
     * 051's profile ladder). */
    void          fail_first(Call call, std::uint32_t count, std::uint32_t code);
    /* The next `call` succeeds with a zero handle or address; a paging queue
     * comes without its fence object. */
    void          zero_next(Call call);
    /* How far the clock moves at each vertical blank; the default is the
     * committed mode's 60 Hz. */
    void          set_blank_period(std::uint64_t ticks);
    /* A wait for a context costs this many ticks, as waiting for real work
     * does; 0 (the default) leaves the clock where it was. */
    void          spend_on_wait(std::uint64_t ticks);
    /* A submission through the kernel costs this many ticks, as the transition
     * into dxgkrnl does; a store to a doorbell costs nothing here, because it
     * is a write in the process and this mock never sees it (feature 049). */
    void          spend_on_submit(std::uint64_t ticks);
    /* Every `nth` wait for a context costs `ticks` more, as one the host was
     * slow to answer does: what a latency's median and fastest are judged by. */
    void          slow_every(std::uint32_t nth, std::uint64_t ticks);
    /* The frequency the performance counter answers; ticks_per_second until
     * set, and 0 is a clock that cannot say how long a tick is. */
    void          set_frequency(std::uint64_t frequency);
    /* Every `nth` blank takes `ticks` longer, as one the waiter missed does. */
    void          slip_every(std::uint32_t nth, std::uint64_t ticks);
    /* From now on a wait on a context created with an RvkContextRecord finds
     * `answer` (up to max_answer bytes) in the record's reply slot, for the
     * fence the last submission named, while the process holds a view of the
     * slot: how a test plays the device behind a transport it cannot reach. */
    void          answer_waits(const std::uint8_t *answer, std::uint32_t size);
    /* The next wait on such a context that finds no earlier queued answer finds
     * this one (up to max_answer bytes), before any answer_waits answers: how a
     * test plays a device through a sequence of calls. */
    void          queue_answer(const std::uint8_t *answer, std::uint32_t size);
    /* The Nth wait for a context from now (1-based) succeeds, writes nothing
     * and takes no queued answer: how a context the host faulted looks to its
     * process, since the driver completes what it queued and never reports
     * DMA_FAULTED, which bugchecks the guest. The driver knows the context
     * lost from then on, and refuses a bind or unbind on it. */
    void          withhold_answer_at(std::uint32_t nth);
    /* The Nth wait for a context from now (1-based) runs `play(context)` before
     * any answer is written: how a test plays work the host does between a
     * submission and its wait, such as a GPU copy between two locked views. */
    void          play_at_wait(std::uint32_t nth, void (*play)(void *context), void *context);
    /* The next escape that names its handles rightly succeeds with these bytes
     * (cut to the escape's size) written over its data, whatever it asked. */
    void          answer_next_escape(const std::uint8_t *answer, std::uint32_t size);
    /* From now on an escape naming only an adapter - no device, no context, as
     * rvkinspect.exe's trace escapes are - is answered by `driver`; without
     * one such an escape is refused as the driver refuses what it does not
     * know. */
    void          answer_adapter_escapes(AdapterEscapes *driver);
    /* The next bind answers `pages` rather than the allocation's own count, as
     * a driver that miscounted would. */
    void          answer_next_bind_pages(std::uint64_t pages);
    /* The data of the last escape as it was sent, and its size. */
    const std::uint8_t *last_escape() const;
    std::uint32_t       last_escape_size() const;
    /* Chunks bound and not yet unbound; the allocation the Nth bind (0-based)
     * named, or 0. */
    std::uint32_t       bindings() const;
    std::uint32_t       bound_allocation(std::uint32_t index) const;
    /* Queued answers no wait has taken yet. */
    std::uint32_t queued_answers() const;
    /* Commands submitted on transports' rings, in order, and the id and size
     * (header included) of the one at `index`, 0 past the journal. */
    std::uint32_t commands() const;
    std::uint32_t command_id(std::uint32_t index) const;
    std::uint32_t command_size(std::uint32_t index) const;
    /* Its first journal_bytes bytes, header first - past a shorter command,
     * whatever the ring held there; null past the journal. */
    const std::uint8_t *command_bytes(std::uint32_t index) const;

    std::uint32_t calls(Call call) const;
    /* Adapters, devices, allocations, queues, contexts and mappings still open. */
    std::uint32_t open_objects() const;
    /* Calls naming a handle this mock never gave or already closed. */
    std::uint32_t misuses() const;
    /* Bytes of the view this mock hands out that are not zero: what was
     * written through the pointer a lock answered. */
    std::uint32_t view_filled() const;
    /* The buffer a locked allocation's view points into, or null: how a test
     * plays the device, reading and writing the guest's mapped memory. */
    std::uint8_t *view_of(std::uint32_t allocation);
    /* The private data of the last create_context_virtual, and its size. */
    const std::uint8_t *last_private() const;
    std::uint32_t       last_private_size() const;
    /* The private data of the last submit, and its size. */
    const std::uint8_t *last_submission() const;
    std::uint32_t       last_submission_size() const;
    /* The AllocationKind a live allocation was created with, or 0. */
    std::uint16_t       allocation_kind(std::uint32_t allocation);
    /* How many pages it was created with, or 0. */
    std::uint64_t       allocation_pages(std::uint32_t allocation);

private:
    enum class Kind : unsigned { none, adapter, device, allocation, queue, context, mapping, hw_queue,
                                doorbell };
    /* One doorbell this mock handed out: the pages a process was given, what
     * it stored, and what its status page says (feature 049). */
    struct DoorbellPages {
        std::uint32_t handle;
        std::uint32_t queue;
        std::uint32_t page;      /* a 4-byte store lands here */
        std::uint32_t status;
        std::uint64_t queued;
        std::uint32_t stores;
        bool          connected;
    };
    /* A context's `address` is the reply allocation its record named, or 0,
     * `ring` the ring allocation, and `lost` whether a wait on it was
     * withheld - how the driver sees a context the host faulted. */
    struct Object {
        Kind          kind;
        std::uint32_t handle;
        std::uint32_t owner;
        std::uint64_t address;
        std::uint64_t pages;
        std::uint16_t allocation_kind;
        std::uint32_t ring;
        bool          lost;
        /* A context created as one a hardware queue may run on: dxgkrnl
         * refuses D3DKMTSubmitCommand for it outright (feature 049). */
        bool          hardware_scheduled;
        /* An allocation keeps the private driver data it was created with, as
         * the runtime does: it is what dxgkrnl hands the opening process, and
         * so the only thing a shared object's name travels on (feature 053). */
        std::uint8_t  created_with[max_allocation_private];
        std::uint32_t created_size;
    };
    /* One NT handle this mock minted: what it names, and the creator's bytes
     * as they were when it was minted. */
    struct SharedHandle {
        std::uint64_t handle;
        std::uint32_t allocation;
        std::uint8_t  carried[max_allocation_private];
        std::uint32_t carried_size;
    };
    static constexpr std::uint32_t max_adapters = 8u;
    /* More than a transport holds: one per context it runs. */
    static constexpr std::uint32_t max_doorbells = 8u;
    static constexpr std::uint32_t segment_groups = 2u;
    static constexpr std::uint64_t first_address = 0x100000000ull;

    DoorbellPages *doorbell_of(std::uint32_t handle);

    bool          begin(Call call);
    Status        refuse(std::uint32_t code);
    Status        succeed();
    bool          zeroed(Call call);
    bool          withheld();
    std::uint32_t open(Kind kind, std::uint32_t owner, std::uint64_t address, std::uint64_t pages);
    Object       *find(Kind kind, std::uint32_t handle);
    Object       *find_mapping(std::uint64_t address);
    bool          is_locked(std::uint32_t allocation) const;
    void          write_answer(const Object &context);
    Status        carry(const Object &context, std::uint64_t commands, std::uint64_t length,
                        const std::uint8_t *private_data, std::uint32_t private_size);
    void          take_doorbell_store(const Object &context);
    void          write_bytes(const Object &context, const std::uint8_t *answer, std::uint32_t size);
    void          journal(const Object &context);
    void          consume(const Object &context);
    bool          played();
    Status        bind_chunk(std::uint32_t device, std::uint8_t *data, std::uint32_t size);
    Status        close(Kind kind, std::uint32_t handle);
    SharedHandle *shared_row(std::uint64_t handle);
    Status        misuse();

    PciIdentity   identities_[max_adapters];
    std::uint32_t versions_[max_adapters];
    std::uint64_t luids_[max_adapters];
    std::uint64_t budgets_[segment_groups];
    std::uint64_t usages_[segment_groups];
    std::uint32_t adapter_count_;
    std::uint32_t claimed_;
    bool          claim_;
    std::uint32_t failures_[static_cast<unsigned>(Call::count)];
    std::uint32_t countdown_[static_cast<unsigned>(Call::count)];
    std::uint32_t failing_run_[static_cast<unsigned>(Call::count)];
    bool          zeroing_[static_cast<unsigned>(Call::count)];
    std::uint32_t calls_[static_cast<unsigned>(Call::count)];
    Object        objects_[max_objects];
    std::uint8_t  views_[max_locks][view_capacity];
    std::uint32_t locked_[max_locks];
    std::uint8_t *large_views_[max_locks];
    std::uint64_t large_bytes_[max_locks];
    std::uint8_t  escape_[max_private];
    std::uint32_t escape_size_;
    std::uint8_t  escape_answer_[max_private];
    std::uint32_t escape_answer_size_;
    bool          escape_scripted_;
    AdapterEscapes *adapter_escapes_;
    std::uint64_t bind_pages_;
    std::uint32_t bound_resources_[max_bindings];
    std::uint32_t bound_allocations_[max_bindings];
    std::uint32_t bound_history_[max_bindings];
    std::uint32_t binds_;
    std::uint32_t play_countdown_;
    void        (*play_)(void *context);
    void         *play_context_;
    std::uint8_t  private_[max_private];
    std::uint32_t private_size_;
    std::uint8_t  submission_[max_private];
    std::uint32_t submission_size_;
    SharedHandle  shared_[max_shared];
    std::uint64_t next_shared_handle_;
    std::uint32_t next_handle_;
    std::uint64_t next_address_;
    std::uint64_t next_fence_;
    std::uint32_t success_code_;
    std::uint32_t last_code_;
    std::uint32_t misuses_;
    std::uint64_t ticks_;
    std::uint64_t blank_period_;
    std::uint32_t slip_nth_;
    std::uint64_t slip_ticks_;
    std::uint64_t wait_cost_;
    std::uint64_t submit_cost_;
    std::uint32_t slow_nth_;
    std::uint64_t slow_ticks_;
    std::uint64_t frequency_;
    std::uint8_t  answer_[max_answer];
    std::uint32_t answer_size_;
    bool          answering_;
    std::uint8_t  queued_[max_queued][max_answer];
    std::uint32_t queued_sizes_[max_queued];
    std::uint32_t queued_count_;
    std::uint32_t queued_taken_;
    std::uint32_t withheld_countdown_;
    /* Feature 049: whether the OS gives this adapter hardware queues, and the
     * doorbells it handed out. */
    bool          hardware_scheduling_;
    std::uint32_t doorbell_page_missing_;
    DoorbellPages doorbells_[max_doorbells];
    std::uint32_t doorbell_count_;
    /* What a hardware queue has finished, as dxgkrnl maps it read-only into
     * the process that made the queue, and the monitored fence object it is
     * reported on: the handle of the queue itself, so a wait names what it
     * waits for. */
    std::uint64_t queue_progress_[max_doorbells];
    std::uint32_t queue_progress_taken_;
    std::uint32_t journal_ids_[max_journal];
    std::uint32_t journal_sizes_[max_journal];
    std::uint8_t  journal_bytes_[max_journal][journal_bytes];
    std::uint32_t journal_count_;
};

}  // namespace av

#endif  // AV_MOCK_KMT_HPP
