#include "harness/executor/mock_memory_exporter.hpp"

#include "executor/exported_memory.hpp"
#include "harness/framework/test_registry.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace av {

MockMemoryExporter::MockMemoryExporter()
    : types_{}, type_count_(0u), heaps_{}, heap_count_(0u), allocations_{}, fail_allocation_(0u), failing_(0u), rounding_(0u), claimed_(0u), attempts_(0u),
      next_handle_(0x1000u), made_(0u), freed_(0u), last_type_(max_types), last_size_(0u), misuses_(0u)
{
}

MockMemoryExporter::~MockMemoryExporter()
{
    if (live() != 0u)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "%u exported allocation(s) never freed", live());
    for (Allocation &allocation : allocations_) {
        if (allocation.handle == 0u) continue;
        ::munmap(allocation.pointer, static_cast<std::size_t>(allocation.size));
        ::close(allocation.fd);
    }
}

std::uint32_t MockMemoryExporter::add_type(std::uint32_t flags, bool exportable, std::uint32_t heap)
{
    if (type_count_ >= max_types) return max_types;
    types_[type_count_] = Type{flags, exportable, false, 0u, 0u, heap};
    return type_count_++;
}

std::uint32_t MockMemoryExporter::add_heap(VkMemoryHeapFlags flags, VkDeviceSize size)
{
    if (heap_count_ >= max_heaps) return max_heaps;
    heaps_[heap_count_] = VkMemoryHeap{size, flags};
    return heap_count_++;
}

void MockMemoryExporter::fail_allocation_at(std::uint32_t nth) { fail_allocation_ = nth; }
void MockMemoryExporter::fail_allocations(std::uint32_t count) { failing_ = count; }
void MockMemoryExporter::round_to(std::uint64_t bytes) { rounding_ = bytes; }
void MockMemoryExporter::claim_types(std::uint32_t count) { claimed_ = count; }

void MockMemoryExporter::hide_on_import(std::uint32_t type)
{
    if (type < type_count_) types_[type].hidden = true;
}

bool MockMemoryExporter::fires(std::uint32_t &countdown)
{
    return countdown != 0u && --countdown == 0u;
}

void MockMemoryExporter::memory_properties(VkPhysicalDeviceMemoryProperties &answer)
{
    answer = VkPhysicalDeviceMemoryProperties{};
    answer.memoryTypeCount = claimed_ != 0u ? claimed_ : type_count_;
    for (std::uint32_t type = 0u; type < type_count_; type++)
        answer.memoryTypes[type] = VkMemoryType{types_[type].flags, types_[type].heap};
    answer.memoryHeapCount = heap_count_ != 0u ? heap_count_ : 1u;
    answer.memoryHeaps[0] = VkMemoryHeap{default_heap_bytes, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    for (std::uint32_t heap = 0u; heap < heap_count_; heap++) answer.memoryHeaps[heap] = heaps_[heap];
}

bool MockMemoryExporter::exportable(std::uint32_t type)
{
    if (type >= type_count_) {
        misuses_++;
        return false;
    }
    types_[type].questions++;
    return types_[type].exportable;
}

bool MockMemoryExporter::imports_host_visible(std::uint32_t type)
{
    if (type >= type_count_) {
        misuses_++;
        return false;
    }
    types_[type].import_questions++;
    return !types_[type].hidden;
}

Status MockMemoryExporter::allocate(std::uint32_t type, std::uint64_t size, ExportedMemory &memory)
{
    attempts_++;
    last_type_ = type;
    last_size_ = size;
    memory = ExportedMemory();
    if (type >= type_count_ || !types_[type].exportable) misuses_++;
    const bool failing = failing_ != 0u;
    if (failing) failing_--;
    if (fires(fail_allocation_) || failing || size == 0u) return Status::no_memory;
    Allocation *slot = nullptr;
    for (Allocation &allocation : allocations_)
        if (allocation.handle == 0u && slot == nullptr) slot = &allocation;
    if (slot == nullptr) return Status::no_memory;
    const std::uint64_t made = rounding_ != 0u ? (size + rounding_ - 1u) / rounding_ * rounding_ : size;
    const int           fd = ::memfd_create("mock exported memory", MFD_CLOEXEC);
    if (fd < 0) return Status::no_memory;
    void *pointer = MAP_FAILED;
    if (::ftruncate(fd, static_cast<off_t>(made)) == 0)
        pointer = ::mmap(nullptr, static_cast<std::size_t>(made), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pointer == MAP_FAILED) {
        ::close(fd);
        return Status::no_memory;
    }
    *slot = Allocation{next_handle_++, fd, static_cast<std::uint8_t *>(pointer), made};
    made_++;
    memory = ExportedMemory(slot->handle, fd, slot->pointer, made);
    return Status::ok;
}

MockMemoryExporter::Allocation *MockMemoryExporter::allocation_of(const ExportedMemory &memory)
{
    for (Allocation &allocation : allocations_)
        if (allocation.handle != 0u && allocation.handle == memory.handle() && allocation.fd == memory.fd())
            return &allocation;
    return nullptr;
}

void MockMemoryExporter::free(const ExportedMemory &memory)
{
    Allocation *const allocation = allocation_of(memory);
    if (allocation == nullptr) {
        misuses_++;
        return;
    }
    ::munmap(allocation->pointer, static_cast<std::size_t>(allocation->size));
    ::close(allocation->fd);
    *allocation = Allocation{};
    freed_++;
}

std::uint32_t MockMemoryExporter::export_questions(std::uint32_t type) const
{
    return type < type_count_ ? types_[type].questions : 0u;
}

std::uint32_t MockMemoryExporter::import_questions(std::uint32_t type) const
{
    return type < type_count_ ? types_[type].import_questions : 0u;
}

std::uint32_t MockMemoryExporter::attempts() const { return attempts_; }
std::uint32_t MockMemoryExporter::allocations() const { return made_; }
std::uint32_t MockMemoryExporter::frees() const { return freed_; }
std::uint32_t MockMemoryExporter::live() const { return made_ - freed_; }
std::uint32_t MockMemoryExporter::last_type() const { return last_type_; }
std::uint64_t MockMemoryExporter::last_size() const { return last_size_; }
std::uint32_t MockMemoryExporter::misuses() const { return misuses_; }

}
