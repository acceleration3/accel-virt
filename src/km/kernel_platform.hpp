#ifndef AV_KERNEL_PLATFORM_HPP
#define AV_KERNEL_PLATFORM_HPP

#include <ntddk.h>

#include "core/platform/platform.hpp"
#include "km/debug_port_log.hpp"

namespace av {

class KernelPlatform final : public Platform {
public:
    explicit KernelPlatform(ULONG pool_tag);
    KernelPlatform(const KernelPlatform &) = delete;
    KernelPlatform &operator=(const KernelPlatform &) = delete;
    ~KernelPlatform();

    void    *allocate(size_t bytes, Pool pool) override;
    void     release(void *block) override;
    void    *allocate_contiguous(size_t bytes) override;
    void     release_contiguous(void *block, size_t bytes) override;
    uint64_t physical_address(void *block) override;
    void    *allocate_pages(uint64_t bytes) override;
    void     release_pages(void *pages) override;
    Status   read_page_frames(void *mdl, uint32_t first, uint32_t count, uint64_t *frames) override;
    uint8_t *map_frame(uint64_t frame) override;
    void     unmap_frame(uint8_t *view) override;
    uint8_t *map_pages(const GuestPageList &pages, void *&mapping) override;
    void     unmap_pages(uint8_t *view, void *mapping) override;
    Status   map_to_process(uint64_t physical, uint64_t bytes, PageCaching caching, uint64_t &view,
                            void *&mapping) override;
    Status   lock_process_pages(uint64_t address, uint64_t bytes, void *&locked) override;
    void     unlock_process_pages(void *locked) override;
    Status   map_pages_to_process(void *pages, uint64_t &view, void *&mapping) override;
    Status   map_pages_to_process_read_only(void *pages, uint64_t &view, void *&mapping) override;
    void     unmap_from_process(void *mapping) override;
    void     zero(void *dst, size_t bytes) override;
    void     copy(void *dst, const void *src, size_t bytes) override;
    Irql     current_irql() const override;
    void     require_irql_at_most(Irql ceiling, const char *function) override;
    Irql     acquire_spin_lock(uintptr_t &word, LockRank rank) override;
    void     release_spin_lock(uintptr_t &word, LockRank rank, Irql previous) override;
    bool     try_acquire_spin_lock(uintptr_t &word, LockRank rank, Irql &previous) override;
    void     initialize_mutex(void *storage) override;
    void     acquire_mutex(void *storage, LockRank rank) override;
    void     release_mutex(void *storage, LockRank rank) override;
    bool     try_acquire_mutex(void *storage, LockRank rank) override;
    void     initialize_wait_mutex(void *storage) override;
    void     acquire_wait_mutex(void *storage, LockRank rank) override;
    void     release_wait_mutex(void *storage, LockRank rank) override;
    uint64_t interrupt_time() const override;
    uint64_t performance_counter(uint64_t &frequency) const override;
    void     store_atomic(uint64_t &target, uint64_t value) override;
    uint64_t add_atomic(uint64_t &target, uint64_t delta) override;
    uint64_t load_atomic(uint64_t &source) override;
    void     stall(uint32_t microseconds) override;
    void     initialize_wake_event(void *storage) override;
    void     set_wake_event(void *storage) override;
    bool     wait_wake_event(void *storage, uint64_t ticks) override;
    uint8_t  read_register_u8(void *address) override;
    uint16_t read_register_u16(void *address) override;
    uint32_t read_register_u32(void *address) override;
    void     write_register_u8(void *address, uint8_t value) override;
    void     write_register_u16(void *address, uint16_t value) override;
    void     write_register_u32(void *address, uint32_t value) override;
    void     write_register_u64(void *address, uint64_t value) override;
    void     log(const char *message) override;
    void     log_at_bugcheck(const char *message) override;
    bool     register_bugcheck(void *buffer, size_t bytes, BugcheckRoutine routine) override;
    void     deregister_bugcheck() override;
    Status   reference_event(uint64_t handle, void *&event) override;
    void     signal_event(void *event) override;
    void     release_event(void *event) override;
    void     report_broken_invariant(const char *what, const char *function) override;
    uint32_t read_service_u32(void *registry_path, const char *name, uint32_t fallback) override;
    int32_t  register_miniport(void *driver_object, void *registry_path,
                               const DdiTable &table) override;
    Status   set_device_description(void *physical_device_object, const uint16_t *text,
                                    uint32_t bytes) override;
    Status   write_driver_text(void *physical_device_object, const char *name, const uint16_t *text,
                               uint32_t bytes) override;
    Status   write_driver_u32(void *physical_device_object, const char *name, uint32_t value) override;
    Status   write_driver_u64(void *physical_device_object, const char *name, uint64_t value) override;

private:
    Status map_section_to_process(void *pages, ACCESS_MASK access, ULONG protection, uint64_t &view,
                                  void *&mapping);

    static constexpr size_t value_name_chars = 64u;

    struct ProcessMapping {
        PMDL      mdl;
        PVOID     view;
        PEPROCESS process;
    };
    struct SectionPages {
        PVOID section;
        PVOID system_view;
    };
    static constexpr uint64_t max_mapping_bytes = 0xFFFFF000u;
    static constexpr uint64_t wow64_address_limit = uint64_t{1} << 32u;

    Status map_mdl_to_process(PMDL mdl, uint64_t bytes, MEMORY_CACHING_TYPE caching, uint64_t &view, void *&mapping);

    Status write_driver_value(void *physical_device_object, const char *name, ULONG type, const void *bytes,
                              ULONG size);

    static constexpr POOL_FLAGS pool_flags_[] = {POOL_FLAG_NON_PAGED, POOL_FLAG_PAGED};
    ULONG        pool_tag_;
    DebugPortLog log_;
    KBUGCHECK_CALLBACK_RECORD bugcheck_record_;
};

}

#endif
