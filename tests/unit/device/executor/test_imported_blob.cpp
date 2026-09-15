/* One guest blob inside one host API. Everything in the handle came from the
 * device, and the device built it from what a guest asked for, so every rule
 * the ABI states is checked before any host API sees it. */
#include "device/abi/rvk_executor.h"
#include "device/executor/imported_blob.hpp"
#include "harness/executor/imported_blob_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ImportedBlob;
using av::ImportedBlobProbe;
using av::Status;

namespace {

std::uint8_t backing[2 * ImportedBlob::page_bytes];

RvkBlobHandle host_pointer(std::uint64_t size)
{ RvkBlobHandle handle = {};
  handle.kind = RVK_BLOB_HOST_POINTER;
  handle.pointer = backing;
  handle.fd = -1;
  handle.size = size;
  return handle; }

RvkBlobHandle dma_buf(int fd, std::uint64_t size)
{ RvkBlobHandle handle = {};
  handle.kind = RVK_BLOB_DMA_BUF;
  handle.fd = fd;
  handle.size = size;
  return handle; }

}  // namespace

TEST(imported_blob, takes_a_host_pointer_and_answers_what_it_holds)
{ ImportedBlob blob;
  CHECK(!blob.live());
  CHECK_STATUS(blob.take(7u, host_pointer(ImportedBlob::page_bytes)), Status::ok);
  CHECK(blob.live());
  CHECK(blob.holds(7u));
  CHECK(!blob.holds(8u));
  CHECK_EQ(blob.kind(), std::uint32_t{RVK_BLOB_HOST_POINTER});
  CHECK_EQ(blob.size(), ImportedBlob::page_bytes);
  CHECK(blob.pointer() == backing);
  CHECK_EQ(blob.descriptor(), -1);
  CHECK(blob.invariants_hold()); }

TEST(imported_blob, takes_a_dma_buf)
{ ImportedBlob blob;
  CHECK_STATUS(blob.take(3u, dma_buf(11, 2u * ImportedBlob::page_bytes)), Status::ok);
  CHECK_EQ(blob.kind(), std::uint32_t{RVK_BLOB_DMA_BUF});
  CHECK_EQ(blob.descriptor(), 11);
  CHECK(blob.pointer() == nullptr);
  CHECK(blob.invariants_hold()); }

/* A size that is not whole pages cannot be imported by Vulkan at all -
 * minImportedHostPointerAlignment is a page on every GPU measured - so it is
 * refused here rather than by a driver later. */
TEST(imported_blob, a_size_that_is_not_whole_pages_is_refused)
{ ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, host_pointer(0u)), Status::invalid_parameter);
  CHECK_STATUS(blob.take(1u, host_pointer(1u)), Status::invalid_parameter);
  CHECK_STATUS(blob.take(1u, host_pointer(ImportedBlob::page_bytes - 1u)),
               Status::invalid_parameter);
  CHECK_STATUS(blob.take(1u, host_pointer(ImportedBlob::page_bytes + 1u)),
               Status::invalid_parameter);
  CHECK(!blob.live());
  CHECK(blob.invariants_hold()); }

/* Exactly one kind: two would mean the device could not decide which API
 * object is about to be made, and none means it decided nothing. */
TEST(imported_blob, a_handle_that_is_no_kind_or_two_is_refused)
{ ImportedBlob blob;
  RvkBlobHandle none = host_pointer(ImportedBlob::page_bytes);
  none.kind = 0u;
  CHECK_STATUS(blob.take(1u, none), Status::invalid_parameter);

  RvkBlobHandle both = host_pointer(ImportedBlob::page_bytes);
  both.kind = RVK_BLOB_HOST_POINTER | RVK_BLOB_DMA_BUF;
  CHECK_STATUS(blob.take(1u, both), Status::invalid_parameter);

  RvkBlobHandle invented = host_pointer(ImportedBlob::page_bytes);
  invented.kind = 4u;
  CHECK_STATUS(blob.take(1u, invented), Status::invalid_parameter);
  CHECK(!blob.live()); }

/* The field the kind does not use must be empty, because a handle carrying
 * both is one nobody can say the meaning of. */
TEST(imported_blob, a_kind_carrying_the_other_kind_s_field_is_refused)
{ ImportedBlob blob;
  RvkBlobHandle pointer_with_fd = host_pointer(ImportedBlob::page_bytes);
  pointer_with_fd.fd = 4;
  CHECK_STATUS(blob.take(1u, pointer_with_fd), Status::invalid_parameter);

  RvkBlobHandle pointer_without_pointer = host_pointer(ImportedBlob::page_bytes);
  pointer_without_pointer.pointer = nullptr;
  CHECK_STATUS(blob.take(1u, pointer_without_pointer), Status::invalid_parameter);

  RvkBlobHandle descriptor_with_pointer = dma_buf(4, ImportedBlob::page_bytes);
  descriptor_with_pointer.pointer = backing;
  CHECK_STATUS(blob.take(1u, descriptor_with_pointer), Status::invalid_parameter);

  CHECK_STATUS(blob.take(1u, dma_buf(-1, ImportedBlob::page_bytes)), Status::invalid_parameter);
  CHECK(!blob.live()); }

/* A host pointer's reserved field is the Vulkan memory type the guest asked
 * for (feature 045), carried to the backend untouched: whether the type exists
 * is the device's to answer, not the ABI's. Every value, the largest included. */
TEST(imported_blob, a_host_pointer_carries_the_memory_type_it_was_asked_into)
{ constexpr std::uint32_t types[] = {0u, 4u, 31u, 32u, 0xFFFFFFFFu};
  for (const std::uint32_t type : types) {
      ImportedBlob blob;
      RvkBlobHandle handle = host_pointer(ImportedBlob::page_bytes);
      handle.reserved = type;
      CHECK_STATUS(blob.take(1u, handle), Status::ok);
      CHECK_EQ(blob.memory_type(), type);
      CHECK(blob.invariants_hold());
  } }

/* A dma-buf has no memory type to ask for, so anything in the field is a
 * handle nobody can say the meaning of. */
TEST(imported_blob, a_dma_buf_with_a_reserved_field_that_is_not_zero_is_refused)
{ ImportedBlob blob;
  RvkBlobHandle handle = dma_buf(11, ImportedBlob::page_bytes);
  handle.reserved = 1u;
  CHECK_STATUS(blob.take(1u, handle), Status::invalid_parameter);
  CHECK(!blob.live());
  CHECK_EQ(blob.memory_type(), 0u); }

TEST(imported_blob, a_slot_already_holding_one_is_not_overwritten)
{ ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, host_pointer(ImportedBlob::page_bytes)), Status::ok);
  CHECK_STATUS(blob.take(2u, host_pointer(ImportedBlob::page_bytes)), Status::invalid_state);
  CHECK(blob.holds(1u)); }

TEST(imported_blob, giving_it_back_leaves_nothing_behind)
{ ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, host_pointer(ImportedBlob::page_bytes)), Status::ok);
  RvkBlobHandle typed = host_pointer(ImportedBlob::page_bytes);
  typed.reserved = 3u;
  blob.give_back();
  CHECK_STATUS(blob.take(1u, typed), Status::ok);
  blob.record(0x1234u, 0x5678u);
  blob.give_back();
  CHECK(!blob.live());
  CHECK(!blob.holds(1u));
  CHECK(blob.pointer() == nullptr);
  CHECK_EQ(blob.descriptor(), -1);
  CHECK_EQ(blob.object(), 0u);
  CHECK_EQ(blob.wire_handle(), 0u);
  CHECK_EQ(blob.memory_type(), 0u);
  CHECK(blob.invariants_hold()); }

/* What the host API made of it: a VkDeviceMemory, a GL memory object, a
 * cl_mem - all of them fit one number, and the backend is what puts it there -
 * and the number the guest names it by, which is none for an API whose guest
 * never names it. */
TEST(imported_blob, remembers_what_the_host_api_made_of_it_and_what_the_guest_names)
{ ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, host_pointer(ImportedBlob::page_bytes)), Status::ok);
  CHECK_EQ(blob.object(), 0u);
  CHECK_EQ(blob.wire_handle(), 0u);
  blob.record(0xDEADBEEFu, 0xFEEDu);
  CHECK_EQ(blob.object(), 0xDEADBEEFu);
  CHECK_EQ(blob.wire_handle(), 0xFEEDu);
  CHECK(blob.invariants_hold());
  blob.record(0xDEADBEEFu, 0u);
  CHECK_EQ(blob.wire_handle(), 0u);
  CHECK(blob.invariants_hold());
  blob.record(0u, 0u);
  CHECK_EQ(blob.object(), 0u);
  CHECK(blob.invariants_hold()); }

/* An empty slot answers pointer() and descriptor() like any other, so anything
 * it still carries is a stale address or a descriptor this library already gave
 * back. One field at a time, on a slot that never held anything. */
TEST(imported_blob_invariants, an_empty_slot_with_a_kind_breaks_them)
{ ImportedBlob blob;
  ImportedBlobProbe(blob).set_kind(RVK_BLOB_HOST_POINTER);
  CHECK(!blob.invariants_hold()); }

TEST(imported_blob_invariants, an_empty_slot_with_a_size_breaks_them)
{ ImportedBlob blob;
  ImportedBlobProbe(blob).set_size(ImportedBlob::page_bytes);
  CHECK(!blob.invariants_hold()); }

TEST(imported_blob_invariants, an_empty_slot_with_a_pointer_breaks_them)
{ ImportedBlob blob;
  ImportedBlobProbe(blob).set_pointer(backing);
  CHECK(!blob.invariants_hold()); }

TEST(imported_blob_invariants, an_empty_slot_with_a_descriptor_breaks_them)
{ ImportedBlob blob;
  ImportedBlobProbe(blob).set_descriptor(11);
  CHECK(!blob.invariants_hold()); }

/* Memory a slot that holds nothing still names is memory a later import would
 * be answered with, and a handle the guest would free twice. */
TEST(imported_blob_invariants, an_empty_slot_with_an_object_breaks_them)
{ ImportedBlob blob;
  blob.record(0x1000u, 0u);
  CHECK(!blob.invariants_hold()); }

TEST(imported_blob_invariants, an_empty_slot_with_a_wire_handle_breaks_them)
{ ImportedBlob blob;
  ImportedBlobProbe(blob).set_wire_handle(0x1000u);
  CHECK(!blob.invariants_hold()); }

TEST(imported_blob_invariants, an_empty_slot_with_a_memory_type_breaks_them)
{ ImportedBlob blob;
  ImportedBlobProbe(blob).set_memory_type(2u);
  CHECK(!blob.invariants_hold()); }

/* A name the guest was given for nothing the host API made is a handle that
 * frees nothing, or something else. */
TEST(imported_blob_invariants, a_wire_handle_with_no_object_breaks_them)
{ ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, host_pointer(ImportedBlob::page_bytes)), Status::ok);
  blob.record(0u, 0x1000u);
  CHECK(!blob.invariants_hold()); }

/* A memory type on a dma-buf is one take would have refused. */
TEST(imported_blob_invariants, a_dma_buf_blob_with_a_memory_type_breaks_them)
{ ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, dma_buf(11, ImportedBlob::page_bytes)), Status::ok);
  ImportedBlobProbe(blob).set_memory_type(1u);
  CHECK(!blob.invariants_hold()); }

/* A live blob of no size, or of part of a page, is one no host API can map;
 * take refuses both, so a live one holding either was never taken. */
TEST(imported_blob_invariants, a_live_blob_of_no_size_breaks_them)
{ ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, host_pointer(ImportedBlob::page_bytes)), Status::ok);
  ImportedBlobProbe(blob).set_size(0u);
  CHECK(!blob.invariants_hold()); }

TEST(imported_blob_invariants, a_live_blob_that_is_not_whole_pages_breaks_them)
{ ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, host_pointer(ImportedBlob::page_bytes)), Status::ok);
  ImportedBlobProbe(blob).set_size(ImportedBlob::page_bytes + 1u);
  CHECK(!blob.invariants_hold()); }

/* A host pointer is what Vulkan imports, so one that is null is a mapping of
 * address zero, and one carrying a descriptor too is two answers to one ask. */
TEST(imported_blob_invariants, a_host_pointer_blob_with_no_pointer_breaks_them)
{ ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, host_pointer(ImportedBlob::page_bytes)), Status::ok);
  ImportedBlobProbe(blob).set_pointer(nullptr);
  CHECK(!blob.invariants_hold()); }

TEST(imported_blob_invariants, a_host_pointer_blob_with_a_descriptor_breaks_them)
{ ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, host_pointer(ImportedBlob::page_bytes)), Status::ok);
  ImportedBlobProbe(blob).set_descriptor(11);
  CHECK(!blob.invariants_hold()); }

/* A dma-buf is handed over as a descriptor alone: a pointer beside it is an
 * address nobody said the meaning of, and no descriptor is nothing to import. */
TEST(imported_blob_invariants, a_dma_buf_blob_with_a_pointer_breaks_them)
{ ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, dma_buf(11, ImportedBlob::page_bytes)), Status::ok);
  ImportedBlobProbe(blob).set_pointer(backing);
  CHECK(!blob.invariants_hold()); }

TEST(imported_blob_invariants, a_dma_buf_blob_with_no_descriptor_breaks_them)
{ ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, dma_buf(11, ImportedBlob::page_bytes)), Status::ok);
  ImportedBlobProbe(blob).set_descriptor(-1);
  CHECK(!blob.invariants_hold()); }

/* A live blob of no kind, or of both, is one no backend knows how to adopt. */
TEST(imported_blob_invariants, a_live_blob_of_a_kind_nobody_defined_breaks_them)
{ ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, host_pointer(ImportedBlob::page_bytes)), Status::ok);
  ImportedBlobProbe(blob).set_kind(RVK_BLOB_HOST_POINTER | RVK_BLOB_DMA_BUF);
  CHECK(!blob.invariants_hold()); }
