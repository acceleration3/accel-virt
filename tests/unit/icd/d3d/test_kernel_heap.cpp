#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "icd/d3d/kernel_heap.hpp"
#include "icd/d3d/kernel_surface.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/allocation_kind.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"

#include <cstdint>
#include <cstring>

using av::AllocationDescriptor;
using av::AllocationKind;
using av::SharedObjectKind;
using av::SharedObjectRecord;
using av::Status;
using av::d3d::KernelHeap;
using av::d3d::KernelSurface;

namespace {

void mint(KernelHeap &heap, std::uint64_t id)
{
    std::uint8_t *const record_bytes = heap.allocation_data() + AllocationDescriptor::encoded_size;
    SharedObjectRecord record;
    CHECK_STATUS(record.decode(record_bytes, SharedObjectRecord::encoded_size), Status::ok);
    record.assign(id, 7u);
    CHECK_STATUS(record.encode(record_bytes, SharedObjectRecord::encoded_size), Status::ok);
}

}

TEST(kernel_heap, a_heap_is_host_memory_of_its_size_rounded_to_pages_naming_the_host_object_and_asking_an_identity)
{ KernelHeap heap(65537u, false, 41u);
  CHECK_STATUS(heap.make(), Status::ok);
  CHECK_EQ(heap.bytes(), 69632u);
  AllocationDescriptor descriptor;
  CHECK_STATUS(descriptor.decode(heap.allocation_data(), AllocationDescriptor::encoded_size), Status::ok);
  CHECK(descriptor.kind() == AllocationKind::host_heap);
  CHECK(!descriptor.cpu_visible());
  CHECK_EQ(descriptor.size(), 69632u);
  CHECK_EQ(descriptor.host_key(), 41u);
  SharedObjectRecord asked;
  CHECK_STATUS(asked.decode(heap.allocation_data() + AllocationDescriptor::encoded_size, SharedObjectRecord::encoded_size),
               Status::ok);
  CHECK(!asked.assigned());
  CHECK(asked.kind().is_memory());
  CHECK_EQ(asked.size(), 69632u);
  CHECK_EQ(heap.key(), 0u); }

TEST(kernel_heap, a_cpu_visible_heap_says_so)
{ KernelHeap heap(4096u, true, 1u);
  CHECK_STATUS(heap.make(), Status::ok);
  AllocationDescriptor descriptor;
  CHECK_STATUS(descriptor.decode(heap.allocation_data(), AllocationDescriptor::encoded_size), Status::ok);
  CHECK(descriptor.cpu_visible()); }

TEST(kernel_heap, a_heap_of_no_bytes_or_past_the_last_page_is_refused)
{ KernelHeap none(0u, false, 1u);
  CHECK_STATUS(none.make(), Status::invalid_parameter);
  KernelHeap past(~std::uint64_t{0}, false, 1u);
  CHECK_STATUS(past.make(), Status::invalid_parameter); }

TEST(kernel_heap, the_identity_the_miniport_mints_is_the_key_and_an_open_finds_the_heap)
{ KernelHeap made(1u << 20u, false, 9u);
  CHECK_STATUS(made.make(), Status::ok);
  mint(made, 0x1234u);
  CHECK_EQ(made.key(), 0x1234u);
  KernelHeap opened;
  CHECK_STATUS(opened.open(made.resource_data(), KernelHeap::resource_bytes, made.allocation_data(),
                           KernelHeap::allocation_bytes),
               Status::ok);
  CHECK_EQ(opened.key(), 0x1234u);
  CHECK_EQ(opened.bytes(), 1u << 20u);
  CHECK_EQ(opened.host_object(), 9u);
  CHECK(!opened.cpu_visible()); }

TEST(kernel_heap, an_open_of_anything_but_this_driver_s_heap_finds_nothing)
{ KernelHeap made(4096u, false, 1u);
  CHECK_STATUS(made.make(), Status::ok);
  mint(made, 0x99u);
  const void *const resource = made.resource_data();
  const void *const allocation = made.allocation_data();
  KernelHeap opened;
  CHECK_STATUS(opened.open(nullptr, KernelHeap::resource_bytes, allocation, KernelHeap::allocation_bytes),
               Status::not_found);
  CHECK_STATUS(opened.open(resource, KernelHeap::resource_bytes - 1u, allocation, KernelHeap::allocation_bytes),
               Status::not_found);
  std::uint64_t foreign[2] = {0x4e4f54u, 0u};
  CHECK_STATUS(opened.open(foreign, sizeof foreign, allocation, KernelHeap::allocation_bytes), Status::not_found);
  CHECK_STATUS(opened.open(resource, KernelHeap::resource_bytes, nullptr, KernelHeap::allocation_bytes),
               Status::malformed);
  KernelSurface surface(16u, 16u, 87u);
  CHECK_STATUS(surface.make(), Status::ok);
  CHECK_STATUS(opened.open(resource, KernelHeap::resource_bytes, surface.allocation_data(),
                           KernelHeap::allocation_bytes),
               Status::not_found);
  CHECK_EQ(opened.key(), 0u); }
