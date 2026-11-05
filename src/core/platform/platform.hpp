#ifndef AV_PLATFORM_HPP
#define AV_PLATFORM_HPP

#include "core/platform/block_source.hpp"
#include "core/platform/lock_rank.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class GuestPageList;

class DdiTable;


using BugcheckRoutine = void (*)(void *buffer, uint32_t bytes);

enum class PageCaching : unsigned { write_back = 0, write_combined = 1 };

class Platform : public BlockSource {
public:
    Platform() = default;
    Platform(const Platform &) = delete;
    Platform &operator=(const Platform &) = delete;

    virtual void    *allocate_contiguous(size_t bytes) = 0;
    virtual void     release_contiguous(void *block, size_t bytes) = 0;
    virtual uint64_t physical_address(void *block) = 0;

    virtual void    *allocate_pages(uint64_t bytes) = 0;
    virtual void     release_pages(void *pages) = 0;

    [[nodiscard]] virtual Status read_page_frames(void *mdl, uint32_t first, uint32_t count,
                                                  uint64_t *frames) = 0;

    virtual uint8_t *map_frame(uint64_t frame) = 0;
    virtual void     unmap_frame(uint8_t *view) = 0;
    virtual uint8_t *map_pages(const GuestPageList &pages, void *&mapping) = 0;
    virtual void     unmap_pages(uint8_t *view, void *mapping) = 0;
    [[nodiscard]] virtual Status map_to_process(uint64_t physical, uint64_t bytes, PageCaching caching,
                                                uint64_t &view, void *&mapping) = 0;
    [[nodiscard]] virtual Status lock_process_pages(uint64_t address, uint64_t bytes, void *&locked) = 0;
    virtual void                 unlock_process_pages(void *locked) = 0;
    [[nodiscard]] virtual Status map_pages_to_process(void *pages, uint64_t &view, void *&mapping) = 0;
    [[nodiscard]] virtual Status map_pages_to_process_read_only(void *pages, uint64_t &view, void *&mapping) = 0;
    virtual void                 unmap_from_process(void *mapping) = 0;
    virtual void zero(void *dst, size_t bytes) = 0;
    virtual void copy(void *dst, const void *src, size_t bytes) = 0;

    virtual Irql current_irql() const = 0;

    virtual Irql acquire_spin_lock(uintptr_t &word, LockRank rank) = 0;
    virtual void release_spin_lock(uintptr_t &word, LockRank rank, Irql previous) = 0;

    [[nodiscard]] virtual bool try_acquire_spin_lock(uintptr_t &word, LockRank rank, Irql &previous) = 0;

    virtual void initialize_mutex(void *storage) = 0;
    virtual void acquire_mutex(void *storage, LockRank rank) = 0;
    virtual void release_mutex(void *storage, LockRank rank) = 0;
    [[nodiscard]] virtual bool try_acquire_mutex(void *storage, LockRank rank) = 0;
    virtual void initialize_wait_mutex(void *storage) = 0;
    virtual void acquire_wait_mutex(void *storage, LockRank rank) = 0;
    virtual void release_wait_mutex(void *storage, LockRank rank) = 0;

    virtual uint64_t interrupt_time() const = 0;
    virtual void     store_atomic(uint64_t &target, uint64_t value) = 0;
    virtual uint64_t load_atomic(uint64_t &source) = 0;
    virtual uint64_t performance_counter(uint64_t &frequency) const = 0;
    virtual void     stall(uint32_t microseconds) = 0;
    virtual void initialize_wake_event(void *storage) = 0;
    virtual void set_wake_event(void *storage) = 0;
    [[nodiscard]] virtual bool wait_wake_event(void *storage, uint64_t ticks) = 0;

    virtual uint8_t  read_register_u8(void *address) = 0;
    virtual uint16_t read_register_u16(void *address) = 0;
    virtual uint32_t read_register_u32(void *address) = 0;
    virtual void     write_register_u8(void *address, uint8_t value) = 0;
    virtual void     write_register_u16(void *address, uint16_t value) = 0;
    virtual void     write_register_u32(void *address, uint32_t value) = 0;
    virtual void     write_register_u64(void *address, uint64_t value) = 0;

    virtual void log(const char *message) = 0;
    virtual void log_at_bugcheck(const char *message) = 0;

    [[nodiscard]] virtual bool register_bugcheck(void *buffer, size_t bytes, BugcheckRoutine routine) = 0;
    virtual void deregister_bugcheck() = 0;

    [[nodiscard]] virtual Status reference_event(uint64_t handle, void *&event) = 0;
    virtual void signal_event(void *event) = 0;
    virtual void release_event(void *event) = 0;

    [[nodiscard]] virtual Status set_device_description(void *physical_device_object, const uint16_t *text,
                                                        uint32_t bytes) = 0;
    [[nodiscard]] virtual Status write_driver_text(void *physical_device_object, const char *name,
                                                   const uint16_t *text, uint32_t bytes) = 0;
    [[nodiscard]] virtual Status write_driver_u32(void *physical_device_object, const char *name,
                                                  uint32_t value) = 0;
    [[nodiscard]] virtual Status write_driver_u64(void *physical_device_object, const char *name,
                                                  uint64_t value) = 0;

    virtual uint32_t current_process_id() = 0;
    virtual size_t   current_process_image(char *name, size_t capacity) = 0;

    virtual uint32_t read_service_u32(void *registry_path, const char *name, uint32_t fallback) = 0;
    virtual int32_t register_miniport(void *driver_object, void *registry_path,
                                      const DdiTable &table) = 0;
    virtual void report_broken_invariant(const char *what, const char *function) = 0;

protected:
    ~Platform() = default;
};

}

#endif
