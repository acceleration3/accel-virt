#ifndef AV_MOCK_MEMORY_EXPORTER_HPP
#define AV_MOCK_MEMORY_EXPORTER_HPP

#include "executor/memory_exporter.hpp"

#include <cstdint>

#include <vulkan/vulkan_core.h>

namespace av {

class MockMemoryExporter final : public MemoryExporter {
public:
    static constexpr std::uint32_t max_types = 32u;
    static constexpr std::uint32_t max_allocations = 64u;
    static constexpr std::uint32_t device_local = 1u;
    static constexpr std::uint32_t host_visible = 2u;
    static constexpr std::uint32_t host_coherent = 4u;
    static constexpr std::uint32_t host_cached = 8u;

    MockMemoryExporter();
    ~MockMemoryExporter();

    std::uint32_t add_type(std::uint32_t flags, bool exportable, std::uint32_t heap = 0u);
    std::uint32_t add_heap(VkMemoryHeapFlags flags, VkDeviceSize size);
    void          fail_allocation_at(std::uint32_t nth);
    void          fail_allocations(std::uint32_t count);
    void          round_to(std::uint64_t bytes);
    void          fill_new(std::uint8_t byte);
    void          claim_types(std::uint32_t count);
    void          hide_on_import(std::uint32_t type);
    void          refuse_alike(std::uint32_t type);

    void          memory_properties(VkPhysicalDeviceMemoryProperties &answer) override;
    bool          exportable(std::uint32_t type) override;
    bool          imports_host_visible(std::uint32_t type) override;
    bool          imports_alike(std::uint32_t type) override;
    Status        allocate(std::uint32_t type, std::uint64_t size, ExportedMemory &memory) override;
    void          free(const ExportedMemory &memory) override;

    std::uint32_t export_questions(std::uint32_t type) const;
    std::uint32_t import_questions(std::uint32_t type) const;
    std::uint32_t attempts() const;
    std::uint32_t allocations() const;
    std::uint32_t frees() const;
    std::uint32_t live() const;
    std::uint32_t last_type() const;
    std::uint64_t last_size() const;
    std::uint32_t misuses() const;

private:
    static constexpr std::uint32_t max_heaps = 16u;
    static constexpr VkDeviceSize  default_heap_bytes = 1ull << 30;

    struct Type {
        std::uint32_t flags;
        bool          exportable;
        bool          hidden;
        std::uint32_t questions;
        std::uint32_t import_questions;
        std::uint32_t heap;
        bool          unalike;
    };
    struct Allocation {
        std::uint64_t handle;
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
    std::uint64_t rounding_;
    std::uint8_t  fill_;
    std::uint32_t claimed_;
    std::uint32_t attempts_;
    std::uint64_t next_handle_;
    std::uint32_t made_;
    std::uint32_t freed_;
    std::uint32_t last_type_;
    std::uint64_t last_size_;
    std::uint32_t misuses_;
};

}

#endif
