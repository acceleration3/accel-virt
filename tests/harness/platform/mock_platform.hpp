#ifndef AV_MOCK_PLATFORM_HPP
#define AV_MOCK_PLATFORM_HPP

#include "core/platform/platform.hpp"
#include "core/platform/register_window.hpp"
#include "harness/platform/violation.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class LogRecord;
class ServiceValue;

class MockPlatform final : public Platform {
public:
    static constexpr std::uint32_t lock_stack_depth = 16u;
    static constexpr std::uint32_t max_register_windows = 16u;

    MockPlatform();
    explicit MockPlatform(std::uint32_t registration_status);
    MockPlatform(std::uint32_t registration_status, LogRecord &record);
    MockPlatform(std::uint32_t registration_status, const ServiceValue &service);
    ~MockPlatform();
    MockPlatform(const MockPlatform &) = delete;
    MockPlatform &operator=(const MockPlatform &) = delete;

    void    *allocate(std::size_t bytes, Pool pool) override;
    void     release(void *block) override;
    void    *allocate_contiguous(std::size_t bytes) override;
    void     release_contiguous(void *block, std::size_t bytes) override;
    std::uint64_t physical_address(void *block) override;
    void         *allocate_pages(std::uint64_t bytes) override;
    void          release_pages(void *pages) override;
    Status        read_page_frames(void *mdl, std::uint32_t first, std::uint32_t count,
                                   std::uint64_t *frames) override;
    std::uint8_t *map_frame(std::uint64_t frame) override;
    void          unmap_frame(std::uint8_t *view) override;
    std::uint8_t *map_pages(const GuestPageList &pages, void *&mapping) override;
    void          unmap_pages(std::uint8_t *view, void *mapping) override;
    Status        map_to_process(std::uint64_t physical, std::uint64_t bytes, PageCaching caching,
                                 std::uint64_t &view, void *&mapping) override;
    Status        lock_process_pages(std::uint64_t address, std::uint64_t bytes, void *&locked) override;
    void          unlock_process_pages(void *locked) override;
    Status        map_pages_to_process(void *pages, std::uint64_t &view, void *&mapping) override;
    Status        map_pages_to_process_read_only(void *pages, std::uint64_t &view, void *&mapping) override;
    void          unmap_from_process(void *mapping) override;
    void     zero(void *dst, std::size_t bytes) override;
    void     copy(void *dst, const void *src, std::size_t bytes) override;
    Irql     current_irql() const override;
    void     require_irql_at_most(Irql ceiling, const char *function) override;
    Irql     acquire_spin_lock(std::uintptr_t &word, LockRank rank) override;
    void     release_spin_lock(std::uintptr_t &word, LockRank rank, Irql previous) override;
    bool     try_acquire_spin_lock(std::uintptr_t &word, LockRank rank, Irql &previous) override;
    void     initialize_mutex(void *storage) override;
    void     acquire_mutex(void *storage, LockRank rank) override;
    void     release_mutex(void *storage, LockRank rank) override;
    bool     try_acquire_mutex(void *storage, LockRank rank) override;
    void     initialize_wait_mutex(void *storage) override;
    void     acquire_wait_mutex(void *storage, LockRank rank) override;
    void     release_wait_mutex(void *storage, LockRank rank) override;
    std::uint64_t interrupt_time() const override;
    std::uint64_t performance_counter(std::uint64_t &frequency) const override;
    void          store_atomic(std::uint64_t &target, std::uint64_t value) override;
    std::uint64_t load_atomic(std::uint64_t &source) override;
    void     stall(std::uint32_t microseconds) override;
    void     initialize_wake_event(void *storage) override;
    void     set_wake_event(void *storage) override;
    bool     wait_wake_event(void *storage, std::uint64_t ticks) override;
    std::uint8_t  read_register_u8(void *address) override;
    std::uint16_t read_register_u16(void *address) override;
    std::uint32_t read_register_u32(void *address) override;
    void     write_register_u8(void *address, std::uint8_t value) override;
    void     write_register_u16(void *address, std::uint16_t value) override;
    void     write_register_u32(void *address, std::uint32_t value) override;
    void     write_register_u64(void *address, std::uint64_t value) override;
    void     log(const char *message) override;
    void     log_at_bugcheck(const char *message) override;
    bool     register_bugcheck(void *buffer, std::size_t bytes, BugcheckRoutine routine) override;
    void     deregister_bugcheck() override;
    Status   reference_event(std::uint64_t handle, void *&event) override;
    void     signal_event(void *event) override;
    void     release_event(void *event) override;
    void     report_broken_invariant(const char *what, const char *function) override;
    std::int32_t register_miniport(void *driver_object, void *registry_path,
                                   const DdiTable &table) override;
    Status   set_device_description(void *physical_device_object, const std::uint16_t *text,
                                    std::uint32_t bytes) override;
    Status   write_driver_text(void *physical_device_object, const char *name, const std::uint16_t *text,
                               std::uint32_t bytes) override;
    Status   write_driver_u32(void *physical_device_object, const char *name, std::uint32_t value) override;
    std::uint32_t current_process_id() override;
    std::size_t   current_process_image(char *name, std::size_t capacity) override;
    std::uint32_t read_service_u32(void *registry_path, const char *name, std::uint32_t fallback) override;
    Status   write_driver_u64(void *physical_device_object, const char *name, std::uint64_t value) override;

    void          refuse_device_write_at(std::uint32_t nth);
    void         *described_device() const;
    bool          device_description_is(const char *ascii) const;
    std::uint32_t device_writes() const;
    bool          driver_text_is(const char *name, const char *ascii) const;
    std::uint64_t driver_number(const char *name) const;
    std::uint32_t driver_value_type(const char *name) const;

    void set_current_process(std::uint32_t id, const char *image);

    void fail_allocation_at(std::uint32_t nth);

    void set_irql(Irql irql);

    void advance_time(std::uint64_t ticks);
    std::uint64_t stalled_ticks() const;
    std::uint64_t slept_ticks() const;

    void          attach_register_window(void *base, RegisterWindow &window);
    void          detach_register_windows(void *base, std::uint64_t length);
    std::uint32_t attached_register_windows() const;

    using StallCallback = void (*)(void *context);
    void on_stall(StallCallback callback, void *context);
    void on_acquire(StallCallback callback, void *context);
    void on_load(StallCallback callback, void *context);

    void *attach_page_frames(const std::uint64_t *frames, std::uint32_t page_count);

    void          lend_process_range(std::uint64_t address, const std::uint64_t *frames, std::uint32_t page_count);
    void          fail_process_lock_at(std::uint32_t nth);
    std::uint32_t process_locks() const;

    void *translate_physical(std::uint64_t physical_address, std::size_t bytes) const;
    std::uint64_t last_contiguous_address() const;

    void hold_elsewhere(LockRank rank);
    void released_elsewhere();

    std::uint64_t make_process_event();
    void          close_process_event(std::uint64_t handle);
    std::uint32_t event_signals(std::uint64_t handle) const;
    std::uint32_t event_references(std::uint64_t handle) const;

    void refuse_bugcheck_registration();
    bool bugcheck();
    bool          bugcheck_registered() const;
    const void   *bugcheck_buffer() const;
    std::size_t   bugcheck_bytes() const;
    std::uint32_t bugcheck_lines() const;
    bool          bugcheck_line(const char *text) const;

    bool claim(Violation kind);

    std::uint32_t unclaimed_violations() const;
    std::uint32_t live_allocations() const;
    std::uint32_t mapped_frames() const;
    void          fail_frame_map_at(std::uint32_t nth);
    std::uint32_t process_mappings() const;
    std::uint32_t read_only_process_mappings() const;
    std::uint32_t write_combined_process_mappings() const;
    bool          maps_into_process(std::uint64_t view, std::uint64_t physical, std::uint64_t bytes) const;
    void          fail_process_map_at(std::uint32_t nth);
    void          fail_page_allocation_at(std::uint32_t nth);
    using PageBacker = void *(*)(void *context, std::uint64_t bytes, std::uint64_t &physical);
    void          back_pages_with(PageBacker backer, void *context);
    std::uint32_t owned_page_blocks() const;
    std::uint32_t page_blocks_made() const;
    std::uint8_t *page_block(std::uint32_t ordinal) const;
    std::uint32_t reclaim_page_blocks();
    using ProcessViewResolver = void *(*)(void *context, std::uint64_t physical, std::uint64_t bytes);
    void          resolve_process_views(ProcessViewResolver resolver, void *context);
    std::uint32_t allocations_made() const;
    std::uint32_t locks_held() const;
    bool          holds(LockRank rank) const;
    std::uint32_t logged() const;
    const char   *last_log() const;
    bool          logged_line(const char *text) const;
    void         *registered_driver_object() const;
    void         *registered_registry_path() const;
    const DdiTable *registered_table() const;

private:
    static constexpr std::uint32_t max_live = 4096u;
    static constexpr std::uint32_t max_contiguous = 1024u;
    static constexpr std::uint32_t max_page_frames = 256u;
    static constexpr std::uint32_t max_frame_views = 256u;
    static constexpr std::uint32_t max_process_views = 256u;
    static constexpr std::uint32_t max_events = 32u;
    static constexpr std::uint64_t first_event_handle = 0x1F0u;
    static constexpr std::uint64_t first_process_view = 0x00007FF000000000ull;
    static constexpr std::uint64_t first_physical_address = 0x100000000ull;
    static constexpr std::uint32_t violation_kinds =
        static_cast<std::uint32_t>(Violation::count);

    struct ContiguousBlock {
        void         *block;
        std::uint64_t physical_address;
        std::size_t   bytes;
        bool          borrowed = false;
    };

    struct MockEvent {
        std::uint64_t handle = 0u;
        std::uint32_t references = 0u;
        std::uint32_t signals = 0u;
        bool          open = false;
    };

    MockEvent *referenced(void *event);
    const MockEvent *event_of(std::uint64_t handle) const;

    struct ProcessView {
        std::uint64_t physical;
        std::uint64_t bytes;
        std::uint64_t view;
        bool          live;
        bool          read_only;
        PageCaching   caching;
    };
    Status map_owned_to_process(void *pages, bool read_only, std::uint64_t &view, void *&mapping);

    struct PageFrames {
        const std::uint64_t *frames;
        std::uint32_t        count;
    };

    struct OwnedPages {
        PageFrames     record;
        void          *block;
        std::uint64_t *frames;
        std::uint64_t  physical;
        std::uint64_t  bytes;
        std::uint32_t  ordinal;
        bool           live;
    };
    static constexpr std::uint32_t max_owned_pages = 512u;

    OwnedPages *find_owned(void *pages);

    struct AttachedWindow {
        std::uintptr_t  base;
        RegisterWindow *window;
    };


    struct HeldLock {
        LockRank rank;
        Irql     previous;
    };

    void record(Violation kind, const char *format, ...);
    void push_lock(LockRank rank, Irql previous);
    bool pop_lock(LockRank rank);
    RegisterWindow *route(void *address, std::uint32_t width, std::uint64_t &offset);

    void         *live_[max_live];
    std::uint32_t live_count_;
    std::uint32_t allocation_sequence_;
    ContiguousBlock contiguous_[max_contiguous];
    std::uint32_t contiguous_count_;
    std::uint64_t next_physical_address_;
    std::uint8_t *frame_views_[max_frame_views] = {};
    struct PageMapping {
        std::uint8_t  *view;
        std::uint64_t *frames;
        std::uint32_t  count;
    };
    static constexpr std::uint32_t max_page_mappings = 16u;
    PageMapping   page_mappings_[max_page_mappings] = {};
    std::uint32_t page_mapping_count_ = 0u;
    std::uint32_t frame_view_count_ = 0u;
    std::uint32_t frame_map_sequence_ = 0u;
    std::uint32_t frame_fail_at_ = 0u;
    ProcessView   process_views_[max_process_views] = {};
    OwnedPages    owned_pages_[max_owned_pages] = {};
    std::uint32_t page_allocation_sequence_ = 0u;
    std::uint32_t page_blocks_made_ = 0u;
    PageBacker    page_backer_ = nullptr;
    void         *page_backer_context_ = nullptr;
    std::uint32_t page_allocation_fail_at_ = 0u;
    std::uint64_t next_process_view_ = first_process_view;
    std::uint32_t process_map_sequence_ = 0u;
    std::uint32_t process_fail_at_ = 0u;
    ProcessViewResolver process_resolver_ = nullptr;
    void         *process_resolver_context_ = nullptr;
    std::uint32_t process_id_ = 4u;
    char          process_image_[64] = "System";
    std::uint32_t fail_at_;
    Irql          irql_;
    HeldLock      held_[lock_stack_depth];
    std::uint32_t held_count_;
    std::uint32_t violations_[violation_kinds];
    std::uint64_t now_;
    bool          verbose_;
    std::uint32_t logged_;
    char          last_log_[192] = {};
    static constexpr std::size_t log_capacity = 8192u;
    char          lines_[log_capacity] = {};
    std::size_t   lines_used_ = 0u;
    LogRecord    *record_ = nullptr;
    const ServiceValue *service_ = nullptr;
    StallCallback acquire_callback_ = nullptr;
    void         *acquire_context_ = nullptr;
    StallCallback load_callback_ = nullptr;
    void         *load_context_ = nullptr;
    bool          holding_elsewhere_ = false;
    LockRank      held_elsewhere_ = LockRank::log_budget;
    MockEvent     events_[max_events] = {};
    std::uint32_t event_count_ = 0u;
    bool          refuse_bugcheck_ = false;
    void         *bugcheck_buffer_ = nullptr;
    std::size_t   bugcheck_bytes_ = 0u;
    BugcheckRoutine bugcheck_routine_ = nullptr;
    std::uint32_t bugcheck_line_count_ = 0u;
    char          bugcheck_text_[log_capacity] = {};
    std::size_t   bugcheck_used_ = 0u;
    std::uint32_t registration_status_;
    void         *registered_driver_object_;
    void         *registered_registry_path_;
    const DdiTable *registered_table_;
    StallCallback  stall_callback_ = nullptr;
    void          *stall_context_ = nullptr;
    std::uint64_t  stalled_ticks_ = 0u;
    std::uint64_t  slept_ticks_ = 0u;
    PageFrames     page_frames_[max_page_frames];
    std::uint32_t  page_frame_count_;
    struct ProcessRange {
        std::uint64_t address;
        PageFrames    frames;
        bool          locked;
    };
    static constexpr std::uint32_t max_process_ranges = 16u;
    ProcessRange   process_ranges_[max_process_ranges] = {};
    std::uint32_t  process_range_count_ = 0u;
    std::uint32_t  process_lock_sequence_ = 0u;
    std::uint32_t  process_lock_fail_at_ = 0u;
    AttachedWindow register_windows_[max_register_windows];
    std::uint32_t  register_window_count_;

    static constexpr std::uint32_t max_driver_values = 16u;
    static constexpr std::size_t   max_value_name = 64u;
    static constexpr std::size_t   max_value_units = 128u;
    static constexpr std::uint32_t registry_text = 1u;
    static constexpr std::uint32_t registry_u32 = 4u;
    static constexpr std::uint32_t registry_u64 = 11u;
    struct DriverValue {
        char          name[max_value_name];
        std::uint32_t type;
        std::uint16_t text[max_value_units];
        std::uint64_t number;
    };
    bool         device_write_refused(const char *function);
    DriverValue *driver_value(const char *name, bool create);
    const DriverValue *find_driver_value(const char *name) const;
    bool         units_are(const std::uint16_t *units, const char *ascii) const;

    std::uint32_t  device_write_sequence_ = 0u;
    std::uint32_t  refuse_device_write_ = 0u;
    std::uint32_t  device_writes_ = 0u;
    void          *described_device_ = nullptr;
    std::uint16_t  description_[max_value_units] = {};
    DriverValue    driver_values_[max_driver_values] = {};
    std::uint32_t  driver_value_count_ = 0u;
};

}

#endif
