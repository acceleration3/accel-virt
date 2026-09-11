/* Devices, allocations and opens driven by an attacker-chosen sequence of
 * DDI calls: descriptors from the input, handle lists mixing live handles,
 * destroyed ones and arbitrary addresses. The tables may refuse anything; they
 * must never touch memory they do not own, must keep their invariants after
 * every call, and must release everything at the end. */
#include "core/adapter/adapter.hpp"
#include "core/ddi/dxgk_allocation_info_record.hpp"
#include "core/ddi/dxgk_open_allocation_info_record.hpp"
#include "core/ddi/dxgkarg_close_allocation_record.hpp"
#include "core/ddi/dxgkarg_create_allocation_record.hpp"
#include "core/ddi/dxgkarg_create_device_record.hpp"
#include "core/ddi/dxgkarg_describe_allocation_record.hpp"
#include "core/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "core/ddi/dxgkarg_open_allocation_record.hpp"
#include "core/display/display_sources.hpp"
#include "core/memory/allocation_table.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/scheduling/device_table.hpp"
#include "harness/device/fake_virtio_gpu_function.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/dxgkrnl/fake_handle_table.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::uint32_t max_handles = 16u;
constexpr std::uint32_t max_list = 4u;

class Harness {
public:
    Harness()
        : dxgkrnl_(platform_, function_), interface_(platform_), adapter_(platform_, &pdo_), segment_(platform_),
          sources_(platform_), allocations_(platform_, segment_), devices_(platform_, allocations_, interface_),
          allocation_handles_{}, kernel_handles_{}, device_handles_{}, open_handles_{}
    {
        if (interface_.initialize(&dxgkrnl_.record()) != av::Status::ok) __builtin_trap();
        if (segment_.initialize(std::int64_t{64} << 20u) != av::Status::ok) __builtin_trap();
    }

    void *pick(void *const *handles, std::uint8_t selector) const
    {
        if (selector >= 0xF0u) return reinterpret_cast<void *>(static_cast<std::uintptr_t>(selector) * 0x1111u);
        return handles[selector % max_handles];
    }

    void create_allocations(av::WireReader &reader)
    {
        const std::uint32_t count = reader.read_u8() % (max_list + 1u);
        av::DxgkAllocationInfoRecord infos[max_list] = {};
        std::uint8_t descriptors[max_list][av::AllocationDescriptor::encoded_size] = {};
        for (std::uint32_t i = 0u; i < max_list; i++) {
            reader.copy_bytes(descriptors[i], sizeof descriptors[i]);
            infos[i].pPrivateDriverData = descriptors[i];
            infos[i].PrivateDriverDataSize = av::AllocationDescriptor::encoded_size;
        }
        av::DxgkargCreateAllocationRecord request = {};
        request.NumAllocations = count;
        request.pAllocationInfo = infos;
        if (allocations_.create(request) != av::Status::ok) return;
        for (std::uint32_t i = 0u; i < count; i++) {
            const std::uint32_t slot = next_ % max_handles;
            next_++;
            allocation_handles_[slot] = infos[i].hAllocation;
            kernel_handles_[slot] = handles_.insert(infos[i].hAllocation);
        }
    }

    void destroy_allocations(av::WireReader &reader)
    {
        void *list[max_list] = {};
        av::DxgkargDestroyAllocationRecord request = {};
        request.NumAllocations = reader.read_u8() % (max_list + 1u);
        request.pAllocationList = list;
        for (std::uint32_t i = 0u; i < max_list; i++) list[i] = pick(allocation_handles_, reader.read_u8());
        static_cast<void>(allocations_.destroy(request));
    }

    void describe(av::WireReader &reader)
    {
        av::DxgkargDescribeAllocationRecord request = {};
        request.hAllocation = pick(allocation_handles_, reader.read_u8());
        static_cast<void>(allocations_.describe(request, sources_));
    }

    void create_device()
    {
        av::DxgkargCreateDeviceRecord request = {};
        if (devices_.create(adapter_, request) == av::Status::ok) device_handles_[next_++ % max_handles] = request.hDevice;
    }

    void destroy_device(av::WireReader &reader)
    {
        static_cast<void>(devices_.destroy(pick(device_handles_, reader.read_u8())));
    }

    void open(av::WireReader &reader)
    {
        av::DxgkOpenAllocationInfoRecord infos[max_list] = {};
        av::DxgkargOpenAllocationRecord request = {};
        void *device = pick(device_handles_, reader.read_u8());
        request.NumAllocations = reader.read_u8() % (max_list + 1u);
        request.pOpenAllocation = infos;
        for (std::uint32_t i = 0u; i < max_list; i++) {
            const std::uint8_t selector = reader.read_u8();
            infos[i].hAllocation = selector >= 0xF0u ? selector : kernel_handles_[selector % max_handles];
        }
        if (devices_.open(device, request) != av::Status::ok) return;
        for (std::uint32_t i = 0u; i < request.NumAllocations; i++)
            open_handles_[next_++ % max_handles] = infos[i].hDeviceSpecificAllocation;
    }

    void close(av::WireReader &reader)
    {
        void *list[max_list] = {};
        av::DxgkargCloseAllocationRecord request = {};
        void *device = pick(device_handles_, reader.read_u8());
        request.NumAllocations = reader.read_u8() % (max_list + 1u);
        request.pOpenHandleList = list;
        for (std::uint32_t i = 0u; i < max_list; i++) list[i] = pick(open_handles_, reader.read_u8());
        static_cast<void>(devices_.close(device, request));
    }

    void check()
    {
        if (!allocations_.invariants_hold() || !devices_.invariants_hold()) __builtin_trap();
        if (platform_.unclaimed_violations() != 0u) __builtin_trap();
    }

private:
    av::MockPlatform          platform_;
    av::FakeVirtioGpuFunction function_;
    av::FakeDxgkrnlAdapter    dxgkrnl_;
    av::FakeHandleTable       handles_;
    av::DxgkrnlInterface      interface_;
    int                       pdo_ = 0;
    av::Adapter               adapter_;
    av::ApertureSegment       segment_;
    av::DisplaySources        sources_;
    av::AllocationTable       allocations_;
    av::DeviceTable           devices_;
    void                     *allocation_handles_[max_handles];
    std::uint32_t             kernel_handles_[max_handles];
    void                     *device_handles_[max_handles];
    void                     *open_handles_[max_handles];
    std::uint32_t             next_ = 0u;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    Harness harness;
    av::WireReader reader(data, size);
    while (reader.remaining() > 0u) {
        switch (reader.read_u8() % 7u) {
        case 0: harness.create_allocations(reader); break;
        case 1: harness.destroy_allocations(reader); break;
        case 2: harness.describe(reader); break;
        case 3: harness.create_device(); break;
        case 4: harness.destroy_device(reader); break;
        case 5: harness.open(reader); break;
        default: harness.close(reader); break;
        }
        harness.check();
    }
    return 0;
}
