/* A host GPU's memory as a test says it is: any memory types with any flags,
 * any of them refusing to export, allocations that fail on request. Each
 * allocation is real - a memfd the mock maps - so a descriptor shared out of
 * it is a file a test can map, duplicate and see closed, and a byte written
 * through one mapping is read through another. */
#ifndef AV_MOCK_MEMORY_EXPORTER_HPP
#define AV_MOCK_MEMORY_EXPORTER_HPP

#include "executor/memory_exporter.hpp"

#include <cstdint>

#include <vulkan/vulkan_core.h>

namespace av {

class MockMemoryExporter final : public MemoryExporter {
public:
    /* VK_MAX_MEMORY_TYPES. */
    static constexpr std::uint32_t max_types = 32u;
    static constexpr std::uint32_t max_allocations = 64u;
    /* VkMemoryPropertyFlagBits, named here so a test needs no loader. */
    static constexpr std::uint32_t device_local = 1u;
    static constexpr std::uint32_t host_visible = 2u;
    static constexpr std::uint32_t host_coherent = 4u;
    static constexpr std::uint32_t host_cached = 8u;

    MockMemoryExporter();
    /* Fails the test with any allocation still live. */
    ~MockMemoryExporter();

    /* Appends memory type with `flags` on `heap`, and whether it exports: its
     * index. */
    std::uint32_t add_type(std::uint32_t flags, bool exportable, std::uint32_t heap = 0u);
    /* Appends a heap: its index. A GPU none was added to has one, device
     * local, which every type is on unless a test says otherwise. */
    std::uint32_t add_heap(VkMemoryHeapFlags flags, VkDeviceSize size);
    /* The Nth allocate, or share, from now fails; and every one of the next
     * `count`. */
    void          fail_allocation_at(std::uint32_t nth);
    void          fail_allocations(std::uint32_t count);
    void          fail_share_at(std::uint32_t nth);
    /* Every allocation is rounded up to a multiple of `bytes`, as a driver
     * rounds one; 0 makes exactly what was asked. */
    void          round_to(std::uint64_t bytes);
    /* memory_properties() claims `count` types from now on, whatever was
     * added: a driver with more types than any caller may index. A type past
     * those added is a misuse to ask about. */
    void          claim_types(std::uint32_t count);
    /* `type`'s dma-buf imports back only as types no importer can map, as
     * NVIDIA's BAR memory does. */
    void          hide_on_import(std::uint32_t type);

    void          memory_properties(VkPhysicalDeviceMemoryProperties &answer) override;
    bool          exportable(std::uint32_t type) override;
    bool          imports_host_visible(std::uint32_t type) override;
    Status        allocate(std::uint32_t type, std::uint64_t size, ExportedMemory &memory) override;
    void          free(const ExportedMemory &memory) override;
    int           share(const ExportedMemory &memory) override;

    /* How many times each type was asked whether it exports, and whether its
     * export imports host-visible. */
    std::uint32_t export_questions(std::uint32_t type) const;
    std::uint32_t import_questions(std::uint32_t type) const;
    /* allocate calls, made or refused, and those made. */
    std::uint32_t attempts() const;
    std::uint32_t allocations() const;
    std::uint32_t frees() const;
    std::uint32_t live() const;
    /* The type and size of the last allocate. */
    std::uint32_t last_type() const;
    std::uint64_t last_size() const;
    /* free or share of memory this never made, or already freed. */
    std::uint32_t misuses() const;

private:
    /* VK_MAX_MEMORY_HEAPS. */
    static constexpr std::uint32_t max_heaps = 16u;
    static constexpr VkDeviceSize  default_heap_bytes = 1ull << 30;

    struct Type {
        std::uint32_t flags;
        bool          exportable;
        bool          hidden;
        std::uint32_t questions;
        std::uint32_t import_questions;
        std::uint32_t heap;
    };
    struct Allocation {
        std::uint64_t handle;   /* 0 for a free slot */
        int           fd;
        std::uint8_t *pointer;
        std::uint64_t size;
    };

    bool        fires(std::uint32_t &countdown);
    Allocation *allocation_of(const ExportedMemory &memory);

    Type          types_[max_types];
    std::uint32_t type_count_;
    VkMemoryHeap  heaps_[max_heaps];
    std::uint32_t heap_count_;
    Allocation    allocations_[max_allocations];
    std::uint32_t fail_allocation_;
    std::uint32_t failing_;
    std::uint32_t fail_share_;
    std::uint64_t rounding_;
    std::uint32_t claimed_;
    std::uint32_t attempts_;
    std::uint64_t next_handle_;
    std::uint32_t made_;
    std::uint32_t freed_;
    std::uint32_t last_type_;
    std::uint64_t last_size_;
    std::uint32_t misuses_;
};

}  // namespace av

#endif  // AV_MOCK_MEMORY_EXPORTER_HPP
