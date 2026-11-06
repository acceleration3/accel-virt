#ifndef AV_MOCK_KMT_HPP
#define AV_MOCK_KMT_HPP

#include "guest/kmt/beam_position.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/kmt/pci_location.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "proto/context/av_device_count.hpp"
#include "proto/escape/context_views_record.hpp"
#include "proto/resources/host_blob_id.hpp"
#include "proto/resources/host_memory_request.hpp"

#include <cstdint>

namespace av {

class HostWindowRecord;

class AdapterEscapes {
public:
    AdapterEscapes() = default;
    AdapterEscapes(const AdapterEscapes &) = delete;
    AdapterEscapes &operator=(const AdapterEscapes &) = delete;
    virtual std::uint32_t answer(std::uint8_t *data, std::uint32_t size) = 0;

protected:
    ~AdapterEscapes() = default;
};

class ClockReader {
public:
    ClockReader(const ClockReader &) = delete;
    ClockReader &operator=(const ClockReader &) = delete;
    virtual void clock_read() = 0;

protected:
    ClockReader() = default;
    ~ClockReader() = default;
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
        create_shared_allocation,
        destroy_shared_allocation,
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
        scan_line,
        escape,
        adapter_luid,
        query_video_memory,
        query_adapter_location,
        query_adapter_name,
        query_umd_driver_version,
        query_segment_sizes,
        query_adapter_features,
        count
    };
    static constexpr std::uint32_t max_objects = 32u;
    static constexpr std::uint32_t invalid_parameter = 0xC000000Du;
    static constexpr std::uint32_t pending = 0x00000103u;
    static constexpr std::uint32_t io_timeout = 0xC00000B5u;
    static constexpr std::uint64_t page_bytes = 4096u;
    static constexpr std::uint64_t ticks_per_second = 10000000u;
    static constexpr std::uint32_t beam_height = 1080u;
    static constexpr std::uint32_t beam_total_lines = 1125u;
    static constexpr std::uint32_t view_capacity = 8192u;
    static constexpr std::uint32_t max_locks = 16u;
    static constexpr std::uint32_t max_private = 64u;
    static constexpr std::uint32_t max_allocation_private = 128u;
    static constexpr std::uint32_t max_shared = 65u;
    static constexpr std::uint64_t first_shared_handle = 0x8000000000000004ull;
    static constexpr std::uint64_t shared_adapter = 0x5A5A0001u;
    static constexpr std::uint64_t first_object_id = 1u;
    static constexpr std::uint32_t max_answer = 1024u;
    static constexpr std::uint32_t not_supported = 0xC00000BBu;
    static constexpr std::uint32_t max_window_chunks = 16u;
    static constexpr std::uint32_t first_window_resource = HostBlobId::first;
    static constexpr std::uint64_t rig_window_bytes = std::uint64_t{2} << 30u;
    static constexpr std::uint32_t not_found = 0xC0000225u;
    static constexpr std::uint64_t first_luid = 0x0000000100000000ull;
    static constexpr std::uint64_t default_local_budget = std::uint64_t{256} << 20u;
    static constexpr std::uint64_t default_non_local_budget = std::uint64_t{1} << 30u;
    static constexpr std::uint32_t max_queued = 320u;
    static constexpr std::uint32_t max_journal = 128u;
    static constexpr std::uint32_t journal_bytes = 128u;

    MockKmt();
    ~MockKmt();
    MockKmt(const MockKmt &) = delete;
    MockKmt &operator=(const MockKmt &) = delete;

    Status        enumerate_adapters(std::uint32_t *adapters, std::uint32_t capacity, std::uint32_t &count) override;
    Status        query_identity(std::uint32_t adapter, PciIdentity &identity) override;
    Status        query_driver_version(std::uint32_t adapter, std::uint32_t &version) override;
    Status        query_native_gpu_fences(std::uint32_t adapter, bool &supported) override;
    Status        query_adapter_features(std::uint32_t adapter, UmdAdapterInfo &info) override;
    Status        query_adapter_location(std::uint32_t adapter, PciLocation &location) override;
    Status        query_adapter_name(std::uint32_t adapter, std::uint16_t *name, std::uint32_t capacity) override;
    Status        query_umd_driver_version(std::uint32_t adapter, std::uint64_t &version) override;
    Status        query_segment_sizes(std::uint32_t adapter, std::uint64_t &dedicated_video,
                                      std::uint64_t &dedicated_system, std::uint64_t &shared_system) override;
    void          sleep_briefly() override;
    Status        close_adapter(std::uint32_t adapter) override;
    Status        adapter_luid(std::uint32_t adapter, std::uint64_t &luid) override;
    Status        primary_display_luid(std::uint64_t &luid) override;
    void          set_primary_display_luid(std::uint64_t luid);
    Status        query_video_memory(std::uint32_t adapter, MemorySegmentGroup group, std::uint64_t &budget,
                                     std::uint64_t &usage) override;
    Status        create_device(std::uint32_t adapter, std::uint32_t &device) override;
    Status        destroy_device(std::uint32_t device) override;
    Status        create_allocation(std::uint32_t device, const std::uint8_t *descriptor, std::uint32_t size,
                                    std::uint32_t &allocation) override;
    Status        destroy_allocation(std::uint32_t device, std::uint32_t allocation) override;
    Status        create_shared_allocation(std::uint32_t device, std::uint8_t *private_data,
                                            std::uint32_t size, std::uint32_t &resource) override;
    Status        share_object(std::uint32_t device, std::uint32_t resource, std::uint64_t &handle) override;
    Status        destroy_shared_allocation(std::uint32_t device, std::uint32_t resource) override;
    Status        open_shared_object(std::uint32_t device, std::uint64_t handle, std::uint32_t &resource,
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

    void          allow_hardware_scheduling();
    void          refuse_hardware_scheduling();
    void          lack_native_gpu_fences();
    void          set_doorbell_status(std::uint32_t doorbell, std::uint32_t status);
    void          answer_doorbell_without_pages(std::uint32_t which);
    std::uint32_t doorbell_page(std::uint32_t doorbell) const;
    void          clear_doorbell_page(std::uint32_t doorbell);
    std::uint64_t queued_fence(std::uint32_t doorbell) const;
    std::uint32_t doorbell_handle(std::uint32_t index) const;
    Status        wait_for_vertical_blank(std::uint32_t adapter, std::uint32_t device, std::uint32_t source) override;
    Status        scan_line(std::uint32_t adapter, std::uint32_t source, BeamPosition &position) override;
    Status        refresh_millihz(std::uint32_t adapter, std::uint32_t source, std::uint32_t &millihz) override;
    Status        escape(std::uint32_t adapter, std::uint32_t device, std::uint32_t context, std::uint8_t *data,
                         std::uint32_t size) override;
    std::uint64_t performance_counter(std::uint64_t &frequency) const override;
    std::uint32_t last_code() const override;

    void          add_adapter(const PciIdentity &identity, std::uint32_t driver_version);
    void          set_identity(std::uint32_t index, const PciIdentity &identity);
    void          describe_adapter(std::uint32_t index, const PciLocation &location, const char *name,
                                   std::uint64_t kmd_version);
    void          set_segment_sizes(std::uint64_t dedicated_video, std::uint64_t dedicated_system,
                                    std::uint64_t shared_system);
    void          set_adapter_features(std::uint32_t index, std::uint64_t features);
    void          set_adapter_luid(std::uint32_t index, std::uint64_t luid);
    void          set_video_memory(MemorySegmentGroup group, std::uint64_t budget, std::uint64_t usage);
    void          claim_adapters(std::uint32_t count);
    void          fail_at(Call call, std::uint32_t nth, std::uint32_t code);
    void          fail_first(Call call, std::uint32_t count, std::uint32_t code);
    void          zero_next(Call call);
    void          zero_at(Call call, std::uint32_t nth);
    void          scribble_the_next_record();
    void          hand_back_a_short_record();
    void          carry_at_the_next_share(const std::uint8_t *bytes, std::uint32_t size);
    void          set_blank_period(std::uint64_t ticks);
    void          set_refresh_millihz(std::uint32_t millihz);
    void          spend_on_wait(std::uint64_t ticks);
    void          time_out_while_consuming(std::uint32_t count);
    void          time_out_while_beating(std::uint32_t count);
    void          time_out_faulted();
    bool          fault_page_of(std::uint32_t context);
    void          time_out_silently(std::uint32_t count);
    void          time_out_while_pulsing(std::uint32_t count);
    void          pulse(std::uint32_t context);
    void          consume_now(std::uint32_t context);
    void          answer_now(std::uint32_t context);
    void          complete_on_page(std::uint32_t context, std::uint64_t fence);
    void          spend_on_submit(std::uint64_t ticks);
    void          slow_every(std::uint32_t nth, std::uint64_t ticks);
    void          set_frequency(std::uint64_t frequency);
    void          reads_clock(ClockReader *reader, std::uint64_t step);
    void          slip_every(std::uint32_t nth, std::uint64_t ticks);
    void          answer_waits(const std::uint8_t *answer, std::uint32_t size);
    void          queue_answer(const std::uint8_t *answer, std::uint32_t size);
    void          queue_too_large(std::uint32_t required);
    void          withhold_answer_at(std::uint32_t nth);
    void          consume_late_at(std::uint32_t nth);
    void          consume_late_at(std::uint32_t nth, std::uint32_t reads);
    void          answer_views_up_to(std::uint16_t version);
    const std::uint8_t *fence_page_of(std::uint32_t context);
    void          play_at_wait(std::uint32_t nth, void (*play)(void *context), void *context);
    void          answer_next_escape(const std::uint8_t *answer, std::uint32_t size);
    void          answer_adapter_escapes(AdapterEscapes *driver);
    void                open_host_window(std::uint64_t bytes);
    void                refuse_next_window_open(std::uint32_t code);
    std::uint32_t       window_chunks() const;
    std::uint32_t       window_opens() const;
    HostMemoryRequest   last_window_request() const;
    HostMemoryRequest   window_request(std::uint32_t resource) const;
    std::uint8_t       *window_chunk_memory(std::uint32_t resource) const;
    void                answer_next_chunk_bytes(std::uint64_t bytes);
    std::uint8_t       *ring_of(std::uint32_t context);
    std::uint8_t       *reply_of(std::uint32_t context);
    std::uint8_t       *ring_named(std::uint32_t name);
    std::uint8_t       *window_base() const;
    void                accept_wake_events();
    std::uint64_t       wake_event() const;
    std::uint32_t       wake_events() const;
    const std::uint8_t *last_escape() const;
    std::uint32_t       last_escape_size() const;
    std::uint32_t queued_answers() const;
    std::uint32_t commands() const;
    std::uint32_t command_id(std::uint32_t index) const;
    std::uint32_t command_size(std::uint32_t index) const;
    const std::uint8_t *command_bytes(std::uint32_t index) const;

    std::uint32_t calls(Call call) const;
    std::uint32_t open_objects() const;
    std::uint32_t misuses() const;
    std::uint32_t sleeps() const;
    std::uint32_t view_filled() const;
    std::uint8_t *view_of(std::uint32_t allocation);
    const std::uint8_t *last_private() const;
    std::uint32_t       last_private_size() const;
    const std::uint8_t *last_submission() const;
    std::uint32_t       last_submission_size() const;
    std::uint16_t       allocation_kind(std::uint32_t allocation);
    std::uint64_t       allocation_pages(std::uint32_t allocation);

private:
    enum class Kind : unsigned { none, adapter, device, allocation, queue, context, mapping, hw_queue,
                                doorbell };
    struct WindowChunk;
    struct DoorbellPages {
        std::uint32_t handle;
        std::uint32_t queue;
        std::uint32_t page;
        std::uint32_t status;
        std::uint64_t queued;
        std::uint32_t stores;
        bool          connected;
    };
    struct Object {
        Kind          kind;
        std::uint32_t handle;
        std::uint32_t owner;
        std::uint64_t address;
        std::uint64_t pages;
        std::uint16_t allocation_kind;
        std::uint32_t ring;
        bool          lost;
        bool          hardware_scheduled;
        std::uint8_t  created_with[max_allocation_private];
        std::uint32_t created_size;
        bool          resource;
        std::uint8_t *ring_memory = nullptr;
        std::uint64_t ring_bytes = 0u;
        std::uint8_t *reply_memory = nullptr;
        std::uint64_t reply_bytes = 0u;
    };
    struct SharedHandle {
        std::uint64_t handle;
        std::uint32_t allocation;
        std::uint8_t  carried[max_allocation_private];
        std::uint32_t carried_size;
    };
    static constexpr std::uint32_t max_adapters = most_av_devices;
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
    void          consume(const Object &context) const;
    void          beat(const Object &context) const;
    void          fault_fence_page(const Object &context) const;
    std::uint8_t *fence_memory(const Object &context) const;
    bool          played();
    Status        context_views(std::uint32_t context, std::uint8_t *data, std::uint32_t size);
    Status        wake_event(const std::uint8_t *data, std::uint32_t size);
    void          map_context_memory(Object &context, std::uint8_t order, std::uint32_t reply_bytes);
    void          unmap_context_memory(Object &context);
    Status        window_chunk(std::uint32_t context, std::uint8_t *data, std::uint32_t size);
    Status        open_window_chunk(std::uint32_t context, std::uint8_t *data, std::uint32_t size,
                                    const HostWindowRecord &request);
    Status        adopt_chunk(std::uint32_t context, std::uint8_t *data, std::uint32_t size,
                              const HostWindowRecord &request);
    std::uint32_t free_window_resource() const;
    std::uint64_t window_fit(std::uint64_t pages) const;
    void          close_window_chunks_of(std::uint32_t context);
    void          clear_chunk(WindowChunk &chunk);
    std::uint64_t answered_bytes(std::uint64_t pages);
    Status        close(Kind kind, std::uint32_t handle);
    SharedHandle *shared_row(std::uint64_t handle);
    Status        misuse();

    PciIdentity   identities_[max_adapters];
    std::uint32_t versions_[max_adapters];
    std::uint64_t luids_[max_adapters];
    PciLocation    locations_[max_adapters];
    char          names_[max_adapters][64] = {};
    std::uint64_t kmd_versions_[max_adapters] = {};
    std::uint64_t adapter_features_[max_adapters] = {};
    std::uint64_t segment_sizes_[3] = {};
    std::uint64_t budgets_[segment_groups];
    std::uint64_t usages_[segment_groups];
    std::uint32_t adapter_count_;
    std::uint32_t claimed_;
    bool          claim_;
    std::uint32_t failures_[static_cast<unsigned>(Call::count)];
    std::uint32_t countdown_[static_cast<unsigned>(Call::count)];
    std::uint32_t failing_run_[static_cast<unsigned>(Call::count)];
    bool          zeroing_[static_cast<unsigned>(Call::count)];
    std::uint32_t zero_countdown_[static_cast<unsigned>(Call::count)] = {};
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
    bool          accepts_wakes_ = false;
    std::uint64_t wake_event_ = 0u;
    std::uint32_t wake_events_ = 0u;
    AdapterEscapes *adapter_escapes_;
    struct WindowChunk {
        std::uint32_t resource;
        std::uint32_t context;
        std::uint64_t first;
        std::uint64_t pages;
        std::uint32_t memory;
        std::uint8_t *lent;
    };
    std::uint8_t *window_;
    std::uint64_t window_pages_;
    WindowChunk   window_chunks_[max_window_chunks];
    std::uint32_t window_opens_;
    std::uint32_t last_window_request_ = 0u;
    std::uint64_t chunk_bytes_answer_ = 0u;
    std::uint32_t window_refusal_;
    std::uint32_t play_countdown_;
    void        (*play_)(void *context);
    void         *play_context_;
    std::uint8_t  private_[max_private];
    std::uint32_t private_size_;
    std::uint8_t  submission_[max_private];
    std::uint32_t submission_size_;
    SharedHandle  shared_[max_shared];
    std::uint64_t next_shared_handle_;
    std::uint64_t next_object_id_;
    bool          scribble_record_;
    bool          short_record_;
    std::uint8_t  carried_next_[max_allocation_private];
    std::uint32_t carried_next_size_;
    bool          carry_next_;
    std::uint32_t next_handle_;
    std::uint64_t next_address_;
    std::uint64_t next_fence_;
    std::uint32_t success_code_;
    std::uint32_t last_code_;
    std::uint32_t misuses_;
    std::uint32_t sleeps_ = 0u;
    std::uint64_t ticks_;
    mutable std::uint64_t read_ticks_;
    static constexpr std::uint32_t spin_reads = 64u;
    mutable std::uint32_t          idle_reads_;
    std::uint64_t                  released_filled_ = 0u;
    std::uint64_t clock_step_;
    ClockReader  *clock_reader_;
    std::uint64_t blank_period_;
    std::uint32_t refresh_millihz_ = 0u;
    std::uint32_t slip_nth_;
    std::uint64_t slip_ticks_;
    std::uint64_t wait_cost_;
    std::uint32_t timeouts_while_consuming_;
    std::uint32_t timeouts_while_beating_;
    std::uint32_t timeouts_while_pulsing_;
    bool          timeout_faulted_;
    std::uint32_t timeouts_silent_;
    std::uint64_t submit_cost_;
    std::uint32_t slow_nth_;
    std::uint64_t slow_ticks_;
    std::uint64_t frequency_;
    std::uint8_t  answer_[max_answer];
    std::uint32_t answer_size_;
    bool          answering_;
    std::uint8_t  queued_[max_queued][max_answer];
    std::uint32_t queued_sizes_[max_queued];
    std::uint32_t queued_too_large_[max_queued];
    std::uint32_t queued_count_;
    std::uint32_t queued_taken_;
    std::uint32_t withheld_countdown_;
    std::uint32_t late_countdown_ = 0u;
    mutable std::uint32_t late_reads_ = 0u;
    std::uint16_t newest_views_ = ContextViewsRecord::with_fence;
    mutable const Object *late_context_ = nullptr;
    bool          hardware_scheduling_;
    bool          native_gpu_fences_ = true;
    std::uint32_t doorbell_page_missing_;
    DoorbellPages doorbells_[max_doorbells];
    std::uint32_t doorbell_count_;
    std::uint64_t queue_progress_[max_doorbells];
    std::uint32_t queue_progress_taken_;
    std::uint32_t journal_ids_[max_journal];
    std::uint32_t journal_sizes_[max_journal];
    std::uint8_t  journal_bytes_[max_journal][journal_bytes];
    std::uint32_t journal_count_;
    std::uint64_t primary_display_luid_ = 0u;
};

}

#endif
