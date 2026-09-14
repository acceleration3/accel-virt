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
 * transport's submission carried is journalled by id and size. */
#ifndef AV_MOCK_KMT_HPP
#define AV_MOCK_KMT_HPP

#include "guest/kmt/kmt.hpp"
#include "guest/kmt/pci_identity.hpp"

#include <cstdint>

namespace av {

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
        destroy_context,
        submit,
        wait_for_context,
        wait_for_vertical_blank,
        count
    };
    static constexpr std::uint32_t max_objects = 32u;
    static constexpr std::uint32_t invalid_parameter = 0xC000000Du;
    static constexpr std::uint32_t pending = 0x00000103u;
    static constexpr std::uint64_t page_bytes = 4096u;
    /* The performance counter this answers, in the 100ns units Windows uses. */
    static constexpr std::uint64_t ticks_per_second = 10000000u;
    /* The memory each lock addresses; several may be held at once. */
    static constexpr std::uint32_t view_capacity = 8192u;
    static constexpr std::uint32_t max_locks = 4u;
    /* The largest context private data this records for a test to read back. */
    static constexpr std::uint32_t max_private = 64u;
    /* The largest answer a test scripts for a transport's context: room for
     * an OpenCL read of 64 floats and its framing. */
    static constexpr std::uint32_t max_answer = 512u;
    /* Answers a test can queue, and commands the journal keeps. */
    static constexpr std::uint32_t max_queued = 64u;
    static constexpr std::uint32_t max_journal = 128u;
    /* The leading bytes of each journalled command a test can read back. */
    static constexpr std::uint32_t journal_bytes = 128u;

    MockKmt();

    Status        enumerate_adapters(std::uint32_t *adapters, std::uint32_t capacity, std::uint32_t &count) override;
    Status        query_identity(std::uint32_t adapter, PciIdentity &identity) override;
    Status        query_driver_version(std::uint32_t adapter, std::uint32_t &version) override;
    Status        close_adapter(std::uint32_t adapter) override;
    Status        create_device(std::uint32_t adapter, std::uint32_t &device) override;
    Status        destroy_device(std::uint32_t device) override;
    Status        create_allocation(std::uint32_t device, const std::uint8_t *descriptor, std::uint32_t size,
                                    std::uint32_t &allocation) override;
    Status        destroy_allocation(std::uint32_t device, std::uint32_t allocation) override;
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
    Status        create_context_virtual(std::uint32_t device, const std::uint8_t *private_data, std::uint32_t size,
                                        std::uint32_t &context) override;
    Status        destroy_context(std::uint32_t context) override;
    Status        submit(std::uint32_t context, std::uint64_t commands, std::uint64_t length,
                         const std::uint8_t *private_data, std::uint32_t private_size) override;
    Status        wait_for_context(std::uint32_t device, std::uint32_t context) override;
    Status        wait_for_vertical_blank(std::uint32_t adapter, std::uint32_t device, std::uint32_t source) override;
    std::uint64_t performance_counter(std::uint64_t &frequency) const override;
    std::uint32_t last_code() const override;

    /* An adapter enumeration opens; up to eight. */
    void          add_adapter(const PciIdentity &identity, std::uint32_t driver_version);
    /* The next enumeration claims this many adapters, whatever the capacity. */
    void          claim_adapters(std::uint32_t count);
    /* The Nth `call` from now (1-based) answers `code`; a success code answers success. */
    void          fail_at(Call call, std::uint32_t nth, std::uint32_t code);
    /* The next `call` succeeds with a zero handle or address; a paging queue
     * comes without its fence object. */
    void          zero_next(Call call);
    /* How far the clock moves at each vertical blank; the default is the
     * committed mode's 60 Hz. */
    void          set_blank_period(std::uint64_t ticks);
    /* A wait for a context costs this many ticks, as waiting for real work
     * does; 0 (the default) leaves the clock where it was. */
    void          spend_on_wait(std::uint64_t ticks);
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
     * DMA_FAULTED, which bugchecks the guest. */
    void          withhold_answer_at(std::uint32_t nth);
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
    enum class Kind : unsigned { none, adapter, device, allocation, queue, context, mapping };
    /* A context's `address` is the reply allocation its record named, or 0,
     * and `ring` the ring allocation. */
    struct Object {
        Kind          kind;
        std::uint32_t handle;
        std::uint32_t owner;
        std::uint64_t address;
        std::uint64_t pages;
        std::uint16_t allocation_kind;
        std::uint32_t ring;
    };
    static constexpr std::uint32_t max_adapters = 8u;
    static constexpr std::uint64_t first_address = 0x100000000ull;

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
    void          write_bytes(const Object &context, const std::uint8_t *answer, std::uint32_t size);
    void          journal(const Object &context);
    void          consume(const Object &context);
    Status        close(Kind kind, std::uint32_t handle);
    Status        misuse();

    PciIdentity   identities_[max_adapters];
    std::uint32_t versions_[max_adapters];
    std::uint32_t adapter_count_;
    std::uint32_t claimed_;
    bool          claim_;
    std::uint32_t failures_[static_cast<unsigned>(Call::count)];
    std::uint32_t countdown_[static_cast<unsigned>(Call::count)];
    bool          zeroing_[static_cast<unsigned>(Call::count)];
    std::uint32_t calls_[static_cast<unsigned>(Call::count)];
    Object        objects_[max_objects];
    std::uint8_t  views_[max_locks][view_capacity];
    std::uint32_t locked_[max_locks];
    std::uint8_t  private_[max_private];
    std::uint32_t private_size_;
    std::uint8_t  submission_[max_private];
    std::uint32_t submission_size_;
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
    std::uint64_t frequency_;
    std::uint8_t  answer_[max_answer];
    std::uint32_t answer_size_;
    bool          answering_;
    std::uint8_t  queued_[max_queued][max_answer];
    std::uint32_t queued_sizes_[max_queued];
    std::uint32_t queued_count_;
    std::uint32_t queued_taken_;
    std::uint32_t withheld_countdown_;
    std::uint32_t journal_ids_[max_journal];
    std::uint32_t journal_sizes_[max_journal];
    std::uint8_t  journal_bytes_[max_journal][journal_bytes];
    std::uint32_t journal_count_;
};

}  // namespace av

#endif  // AV_MOCK_KMT_HPP
