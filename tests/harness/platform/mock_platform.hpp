/* The adversarial platform. Not a stub: it enforces every rule the platform
 * contract states, in every unit test, and a rule broken without the test
 * claiming it fails the test when the mock is destroyed.
 *
 * One instance per test, constructed on the stack. No globals, no reset hook.
 * Tests are single-threaded, so the lock stack models one thread. */
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
    static constexpr std::uint32_t max_timers = 8u;

    MockPlatform();
    /* The NTSTATUS register_miniport answers, as dxgkrnl would. */
    explicit MockPlatform(std::uint32_t registration_status);
    /* The same, with every line also written where it outlives this platform. */
    MockPlatform(std::uint32_t registration_status, LogRecord &record);
    /* The same, with one value under the service key for DriverEntry to read. */
    MockPlatform(std::uint32_t registration_status, const ServiceValue &service);
    ~MockPlatform();
    MockPlatform(const MockPlatform &) = delete;
    MockPlatform &operator=(const MockPlatform &) = delete;

    void    *allocate(std::size_t bytes, Pool pool) override;
    void     release(void *block) override;
    void    *allocate_contiguous(std::size_t bytes) override;
    void     release_contiguous(void *block, std::size_t bytes) override;
    std::uint64_t physical_address(void *block) override;
    Status        read_page_frames(void *mdl, std::uint32_t first, std::uint32_t count,
                                   std::uint64_t *frames) override;
    /* A frame inside a contiguous block maps to that block's own memory; any
     * other is refused with null. A view left mapped fails the test. */
    std::uint8_t *map_frame(std::uint64_t frame) override;
    void          unmap_frame(std::uint8_t *view) override;
    void     zero(void *dst, std::size_t bytes) override;
    void     copy(void *dst, const void *src, std::size_t bytes) override;
    Irql     current_irql() const override;
    void     require_irql_at_most(Irql ceiling, const char *function) override;
    Irql     acquire_spin_lock(std::uintptr_t &word, LockRank rank) override;
    void     release_spin_lock(std::uintptr_t &word, LockRank rank, Irql previous) override;
    /* Taken as acquire_spin_lock takes it, unless the test holds that rank
     * elsewhere. */
    bool     try_acquire_spin_lock(std::uintptr_t &word, LockRank rank, Irql &previous) override;
    void     initialize_mutex(void *storage) override;
    void     acquire_mutex(void *storage, LockRank rank) override;
    void     release_mutex(void *storage, LockRank rank) override;
    bool     try_acquire_mutex(void *storage, LockRank rank) override;
    void     initialize_wait_mutex(void *storage) override;
    void     acquire_wait_mutex(void *storage, LockRank rank) override;
    void     release_wait_mutex(void *storage, LockRank rank) override;
    bool     try_acquire_wait_mutex(void *storage, LockRank rank) override;
    std::uint64_t interrupt_time() const override;
    /* The test clock, at 10 000 000 ticks per second. */
    std::uint64_t performance_counter(std::uint64_t &frequency) const override;
    /* One thread: a plain store and load. */
    void          store_atomic(std::uint64_t &target, std::uint64_t value) override;
    std::uint64_t load_atomic(std::uint64_t &source) override;
    void    *create_timer(TimerCallback callback, void *context) override;
    void     start_timer(void *timer, std::uint64_t period) override;
    void     stop_timer(void *timer) override;
    void     destroy_timer(void *timer) override;
    void     stall(std::uint32_t microseconds) override;
    std::uint8_t  read_register_u8(void *address) override;
    std::uint16_t read_register_u16(void *address) override;
    std::uint32_t read_register_u32(void *address) override;
    void     write_register_u8(void *address, std::uint8_t value) override;
    void     write_register_u16(void *address, std::uint16_t value) override;
    void     write_register_u32(void *address, std::uint32_t value) override;
    void     write_register_u64(void *address, std::uint64_t value) override;
    void     log(const char *message) override;
    /* Kept apart from log(): a test reads what a bugcheck wrote. */
    void     log_at_bugcheck(const char *message) override;
    bool     register_bugcheck(void *buffer, std::size_t bytes, BugcheckRoutine routine) override;
    void     deregister_bugcheck() override;
    void     report_broken_invariant(const char *what, const char *function) override;
    std::int32_t register_miniport(void *driver_object, void *registry_path,
                                   const DdiTable &table) override;
    Status   set_device_description(void *physical_device_object, const std::uint16_t *text,
                                    std::uint32_t bytes) override;
    Status   write_driver_text(void *physical_device_object, const char *name, const std::uint16_t *text,
                               std::uint32_t bytes) override;
    Status   write_driver_u32(void *physical_device_object, const char *name, std::uint32_t value) override;
    std::uint32_t read_service_u32(void *registry_path, const char *name, std::uint32_t fallback) override;
    Status   write_driver_u64(void *physical_device_object, const char *name, std::uint64_t value) override;

    /* The Nth device property or driver-key write from now (1-based) is
     * refused with not_supported, as the PnP manager refuses one; 0 disables. */
    void          refuse_device_write_at(std::uint32_t nth);
    /* The device whose description was last set, and whether it is `ascii`
     * exactly (UTF-16, NUL-terminated, `bytes` counting the NUL). */
    void         *described_device() const;
    bool          device_description_is(const char *ascii) const;
    std::uint32_t device_writes() const;
    /* The driver-key value `name` as last written: a REG_SZ equal to `ascii`,
     * or a number of either width, and its REG_ type (1, 4 or 11; 0 when never
     * written). */
    bool          driver_text_is(const char *name, const char *ascii) const;
    std::uint64_t driver_number(const char *name) const;
    std::uint32_t driver_value_type(const char *name) const;

    /* Fail the Nth allocation from now (1-based); 0 disables. This is how
     * out-of-memory paths reach full branch coverage instead of being assumed
     * correct. */
    void fail_allocation_at(std::uint32_t nth);

    /* Simulates the IRQL a DDI is entered at. */
    void set_irql(Irql irql);

    void advance_time(std::uint64_t ticks);
    /* Advances the clock by `ticks`, stopping at each running timer's due time
     * to call it at DISPATCH, earliest first. The only way a timer fires. */
    void run_timers(std::uint64_t ticks);

    /* Register accesses from `base` to base + window.length() reach `window`,
     * the way a mapped BAR reaches the device. Anything else is unmapped. */
    void          attach_register_window(void *base, RegisterWindow &window);
    /* Every window whose base lies in [base, base + length). */
    void          detach_register_windows(void *base, std::uint64_t length);
    std::uint32_t attached_register_windows() const;

    /* What the device does while the driver stalls waiting for it: the only
     * point a test can answer a request the driver is blocked on. */
    using StallCallback = void (*)(void *context);
    void on_stall(StallCallback callback, void *context);
    /* Called inside every blocking lock acquire, before the lock is taken: the
     * moment a thread is waiting on it. */
    void on_acquire(StallCallback callback, void *context);

    /* An MDL of `page_count` pages with those frame addresses, as dxgkrnl hands
     * one to BuildPagingBuffer. The frames must outlive this platform; any
     * other pointer is an MDL this platform never made, and is refused. */
    void *attach_page_frames(const std::uint64_t *frames, std::uint32_t page_count);

    /* What a device sees at a fake physical address: the host pointer, if the
     * whole range lies inside one contiguous block. */
    void *translate_physical(std::uint64_t physical_address, std::size_t bytes) const;

    /* Every try_acquire of a lock of `rank` fails, as if another thread held
     * it, until released_elsewhere. */
    void hold_elsewhere(LockRank rank);
    void released_elsewhere();

    /* The next register_bugcheck is refused. */
    void refuse_bugcheck_registration();
    /* The kernel bugchecks: the registered routine runs with its buffer at
     * above DISPATCH; false with nothing registered. */
    bool bugcheck();
    bool          bugcheck_registered() const;
    const void   *bugcheck_buffer() const;
    std::size_t   bugcheck_bytes() const;
    std::uint32_t bugcheck_lines() const;
    /* Whether any line written at a bugcheck holds `text`. */
    bool          bugcheck_line(const char *text) const;

    /* Removes one recorded occurrence of `kind`; false if there was none. */
    bool claim(Violation kind);

    std::uint32_t unclaimed_violations() const;
    std::uint32_t live_allocations() const;
    std::uint32_t mapped_frames() const;
    /* Makes the Nth map_frame from now (1-based) answer null; 0 disables. */
    void          fail_frame_map_at(std::uint32_t nth);
    std::uint32_t allocations_made() const;
    std::uint32_t locks_held() const;
    std::uint32_t logged() const;
    /* The last message logged, empty before the first, cut at its capacity. */
    const char   *last_log() const;
    /* Whether any line logged so far holds `text`: a line written in the middle
     * of a DDI is not the last one by the time the test looks. */
    bool          logged_line(const char *text) const;
    void         *registered_driver_object() const;
    void         *registered_registry_path() const;
    const DdiTable *registered_table() const;

private:
    static constexpr std::uint32_t max_live = 4096u;
    static constexpr std::uint32_t max_contiguous = 256u;
    static constexpr std::uint32_t max_page_frames = 256u;
    static constexpr std::uint32_t max_frame_views = 256u;
    static constexpr std::uint64_t first_physical_address = 0x100000000ull;
    static constexpr std::uint32_t violation_kinds =
        static_cast<std::uint32_t>(Violation::count);

    struct ContiguousBlock {
        void         *block;
        std::uint64_t physical_address;
        std::size_t   bytes;
    };

    struct PageFrames {
        const std::uint64_t *frames;
        std::uint32_t        count;
    };

    struct AttachedWindow {
        std::uintptr_t  base;
        RegisterWindow *window;
    };

    struct Timer {
        TimerCallback callback;
        void         *context;
        std::uint64_t period;
        std::uint64_t due;
        bool          live;
        bool          running;
    };

    /* The live timer `timer` names, or null. */
    Timer *find_timer(void *timer);

    struct HeldLock {
        LockRank rank;
        Irql     previous;
    };

    void record(Violation kind, const char *format, ...);
    void push_lock(LockRank rank, Irql previous);
    bool pop_lock(LockRank rank);
    /* The window holding [address, address + width), and the offset in it. */
    RegisterWindow *route(void *address, std::uint32_t width, std::uint64_t &offset);

    void         *live_[max_live];
    std::uint32_t live_count_;
    std::uint32_t allocation_sequence_;
    ContiguousBlock contiguous_[max_contiguous];
    std::uint32_t contiguous_count_;
    std::uint64_t next_physical_address_;
    std::uint8_t *frame_views_[max_frame_views] = {};
    std::uint32_t frame_view_count_ = 0u;
    std::uint32_t frame_map_sequence_ = 0u;
    std::uint32_t frame_fail_at_ = 0u;
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
    bool          holding_elsewhere_ = false;
    LockRank      held_elsewhere_ = LockRank::log_budget;
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
    Timer          timers_[max_timers];
    StallCallback  stall_callback_ = nullptr;
    void          *stall_context_ = nullptr;
    PageFrames     page_frames_[max_page_frames];
    std::uint32_t  page_frame_count_;
    AttachedWindow register_windows_[max_register_windows];
    std::uint32_t  register_window_count_;

    static constexpr std::uint32_t max_driver_values = 16u;
    static constexpr std::size_t   max_value_name = 64u;
    static constexpr std::size_t   max_value_units = 128u;
    static constexpr std::uint32_t registry_text = 1u;     /* REG_SZ */
    static constexpr std::uint32_t registry_u32 = 4u;      /* REG_DWORD */
    static constexpr std::uint32_t registry_u64 = 11u;     /* REG_QWORD */
    struct DriverValue {
        char          name[max_value_name];
        std::uint32_t type;
        std::uint16_t text[max_value_units];
        std::uint64_t number;
    };
    /* Whether this write is the one refuse_device_write_at named; records a
     * write above PASSIVE. */
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

}  // namespace av

#endif  // AV_MOCK_PLATFORM_HPP
