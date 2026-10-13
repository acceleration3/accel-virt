/* The device's answers on the export socket, and its one event: a 32-byte
 * head, then the body the operation decides - a greeting, a layout, a count
 * of writes or a reason - and one descriptor beside an export or a sync
 * answered ok. A consumer reading one holds it to the same rules the device
 * writes it by, so what one side refuses to write the other refuses to read. */
#include "core/support/status.hpp"
#include "harness/framework/hostile_sweep.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/export/export_answer.hpp"
#include "proto/export/export_request.hpp"

#include <cstdint>
#include <cstring>

using av::ExportAnswer;
using av::ExportGreeting;
using av::ExportOperation;
using av::ExportRequest;
using av::ExportRevocation;
using av::ExportedLayout;
using av::Status;

namespace {

constexpr std::uint64_t surface = 0x5100000000000077ull;
constexpr std::uint64_t tag = 0xCAFEu;
constexpr std::uint64_t implicit = 0x00FFFFFFFFFFFFFFull;
const std::uint8_t uuid[ExportGreeting::uuid_bytes] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

std::uint32_t u32_at(const std::uint8_t *bytes, std::size_t at)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

std::uint64_t u64_at(const std::uint8_t *bytes, std::size_t at)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

ExportAnswer exported()
{
    ExportAnswer answer(ExportRequest(ExportOperation::export_surface, surface, tag), Status::ok);
    answer.describe(ExportedLayout(1003u, 701u, 0x34325241u, 4096u, 0u, implicit));
    return answer;
}

bool rejects_export(const std::uint8_t *bytes, std::size_t length)
{
    ExportAnswer answer;
    return answer.decode(bytes, length, 1u) != Status::ok;
}

}  // namespace

TEST(export_answer, an_export_carries_its_layout_at_the_documented_offsets_and_one_descriptor)
{ std::uint8_t bytes[ExportAnswer::encoded_size] = {};
  const ExportAnswer sent = exported();
  CHECK_EQ(sent.descriptors(), 1u);
  CHECK_STATUS(sent.encode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(u32_at(bytes, 0u), 0x41585641u);
  CHECK_EQ(u32_at(bytes, 4u), 0x00010001u);   /* major 1, minor 1 */
  CHECK_EQ(u32_at(bytes, 8u), 2u);
  CHECK_EQ(u32_at(bytes, 12u), 0u);
  CHECK_EQ(u64_at(bytes, 16u), surface);
  CHECK_EQ(u64_at(bytes, 24u), tag);
  CHECK_EQ(u32_at(bytes, 32u), 1003u);
  CHECK_EQ(u32_at(bytes, 36u), 701u);
  CHECK_EQ(u32_at(bytes, 40u), 0x34325241u);
  CHECK_EQ(u32_at(bytes, 44u), 4096u);
  CHECK_EQ(u32_at(bytes, 48u), 0u);
  CHECK_EQ(u32_at(bytes, 52u), 0u);
  CHECK_EQ(u64_at(bytes, 56u), implicit);
  ExportAnswer read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed);   /* the dma-buf did not come */
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 2u), Status::malformed);
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 1u), Status::ok);
  CHECK(read.operation() == ExportOperation::export_surface);
  CHECK_STATUS(read.status(), Status::ok);
  CHECK_EQ(read.surface(), surface);
  CHECK_EQ(read.tag(), tag);
  CHECK_EQ(read.layout().stride(), 4096u);
  CHECK_EQ(read.layout().modifier(), implicit);
  CHECK_EQ(read.writes(), 0u);
  CHECK_EQ(read.reason(), 0u);
  std::uint8_t small[ExportAnswer::encoded_size - 1u] = {};
  CHECK_STATUS(sent.encode(small, sizeof small), Status::too_large); }

TEST(export_answer, a_sync_carries_the_writes_and_a_sync_file)
{ std::uint8_t bytes[ExportAnswer::encoded_size] = {};
  ExportAnswer sent(ExportRequest(ExportOperation::sync, surface, tag), Status::ok);
  sent.count(0x0102030405060708ull);
  CHECK_STATUS(sent.encode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(u32_at(bytes, 8u), 3u);
  CHECK_EQ(u64_at(bytes, 32u), 0x0102030405060708ull);
  for (std::size_t at = 40u; at < ExportAnswer::encoded_size; at++) CHECK_EQ(bytes[at], 0u);
  ExportAnswer read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 1u), Status::ok);
  CHECK_EQ(read.writes(), 0x0102030405060708ull);
  CHECK_EQ(read.descriptors(), 1u);
  bytes[63] = 1u;
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 1u), Status::malformed); }

TEST(export_answer, hello_carries_the_gpu_and_its_address_only_when_on_pci)
{ std::uint8_t bytes[ExportAnswer::encoded_size] = {};
  ExportAnswer sent(ExportRequest(ExportOperation::hello, 0u, tag), Status::ok);
  sent.greet(ExportGreeting(uuid, 0u, 0x03u, 0x00u, 0x01u));
  CHECK_EQ(sent.descriptors(), 0u);
  CHECK_STATUS(sent.encode(bytes, sizeof bytes), Status::ok);
  CHECK(std::memcmp(bytes + 32u, uuid, sizeof uuid) == 0);
  CHECK_EQ(u32_at(bytes, 48u), 0u);
  CHECK_EQ(bytes[52], 0x03u);
  CHECK_EQ(bytes[53], 0x00u);
  CHECK_EQ(bytes[54], 0x01u);
  CHECK_EQ(bytes[55], 1u);   /* on PCI */
  CHECK_EQ(u32_at(bytes, 56u), 0u);
  CHECK_EQ(u32_at(bytes, 60u), 0u);
  ExportAnswer read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK(read.greeting().on_pci());
  CHECK_EQ(read.greeting().function(), 0x01u);
  /* Both words past the address are reserved. */
  bytes[56] = 1u;
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed);
  bytes[56] = 0u;
  bytes[60] = 1u;
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed);
  bytes[60] = 0u;
  bytes[55] = 0u;   /* off PCI, with an address left behind */
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed);
  bytes[52] = 0u;
  bytes[54] = 0u;
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK(!read.greeting().on_pci());
  bytes[55] = 2u;   /* a flag this version does not have */
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed);
  bytes[55] = 0u;
  bytes[60] = 1u;
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed);
  bytes[60] = 0u;
  std::memset(bytes + 32u, 0, 16u);   /* no UUID: no GPU named */
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed);
  ExportAnswer named(ExportRequest(ExportOperation::export_surface, surface, tag), Status::ok);
  named.greet(ExportGreeting(uuid));   /* a greeting in an export */
  CHECK_STATUS(named.encode(bytes, sizeof bytes), Status::invalid_parameter); }

/* A refusal carries nothing: no body, no descriptor, whatever was asked. */
TEST(export_answer, a_refusal_carries_no_body_and_no_descriptor)
{ const ExportOperation operations[] = {ExportOperation::hello, ExportOperation::export_surface,
                                        ExportOperation::sync, ExportOperation::release};
  const Status statuses[] = {Status::malformed, Status::not_supported, Status::not_found, Status::no_space,
                             Status::invalid_state};
  for (const ExportOperation operation : operations)
      for (const Status status : statuses) {
          std::uint8_t bytes[ExportAnswer::encoded_size] = {};
          const ExportRequest asked(operation, operation == ExportOperation::hello ? 0u : surface, tag);
          const ExportAnswer  sent(asked, status);
          CHECK_EQ(sent.descriptors(), 0u);
          CHECK_STATUS(sent.encode(bytes, sizeof bytes), Status::ok);
          for (std::size_t at = 32u; at < ExportAnswer::encoded_size; at++) CHECK_EQ(bytes[at], 0u);
          ExportAnswer read;
          CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::ok);
          CHECK_STATUS(read.status(), status);
          CHECK_STATUS(read.decode(bytes, sizeof bytes, 1u), Status::malformed);
          bytes[40] = 1u;
          CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed);
      }
  ExportAnswer counted(ExportRequest(ExportOperation::sync, surface, tag), Status::not_found);
  counted.count(1u);
  std::uint8_t bytes[ExportAnswer::encoded_size] = {};
  CHECK_STATUS(counted.encode(bytes, sizeof bytes), Status::invalid_parameter);
  ExportAnswer described(ExportRequest(ExportOperation::export_surface, surface, tag), Status::no_space);
  described.describe(ExportedLayout(1u, 1u, 1u, 4u, 0u, 0u));
  CHECK_STATUS(described.encode(bytes, sizeof bytes), Status::invalid_parameter);
  /* A status no answer has, and one this version does not number. */
  CHECK_STATUS(ExportAnswer(ExportRequest(ExportOperation::release, surface, tag), Status::conflict)
                   .encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(ExportAnswer(ExportRequest(ExportOperation::release, surface, tag), Status::ok).encode(bytes, sizeof bytes),
               Status::ok);
  bytes[12] = 6u;
  ExportAnswer read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed); }

TEST(export_answer, an_export_answered_ok_must_describe_a_picture)
{ std::uint8_t bytes[ExportAnswer::encoded_size] = {};
  ExportAnswer sent(ExportRequest(ExportOperation::export_surface, surface, tag), Status::ok);
  CHECK_STATUS(sent.encode(bytes, sizeof bytes), Status::invalid_parameter);
  sent.describe(ExportedLayout(64u, 64u, 0x34325241u, 63u, 0u, 0u));
  CHECK_STATUS(sent.encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(exported().encode(bytes, sizeof bytes), Status::ok);
  bytes[52] = 1u;   /* the layout's reserved word */
  ExportAnswer read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 1u), Status::malformed);
  bytes[52] = 0u;
  std::memset(bytes + 16u, 0, 8u);   /* no surface */
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 1u), Status::malformed); }

/* The event: a surface the guest released, or every one - 0 - at a reset,
 * never a tag, always ok. */
TEST(export_answer, a_revocation_names_its_surface_and_its_reason)
{ std::uint8_t bytes[ExportAnswer::encoded_size] = {};
  CHECK_STATUS(ExportAnswer(ExportRevocation::released, surface).encode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(u32_at(bytes, 8u), 0x80u);
  CHECK_EQ(u64_at(bytes, 16u), surface);
  CHECK_EQ(u64_at(bytes, 24u), 0u);
  CHECK_EQ(u32_at(bytes, 32u), 1u);
  ExportAnswer read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK(read.operation() == ExportOperation::revoked);
  CHECK_EQ(read.reason(), 1u);
  CHECK_EQ(read.surface(), surface);
  CHECK_STATUS(ExportAnswer(ExportRevocation::reset, 0u).encode(bytes, sizeof bytes), Status::ok);
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK_EQ(read.reason(), 2u);
  CHECK_EQ(read.surface(), 0u);
  CHECK_STATUS(ExportAnswer(ExportRevocation::reset, surface).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(ExportAnswer(ExportRevocation::released, 0u).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(ExportAnswer(static_cast<ExportRevocation>(3u), surface).encode(bytes, sizeof bytes),
               Status::invalid_parameter);
  CHECK_STATUS(ExportAnswer(ExportRevocation::released, surface).encode(bytes, sizeof bytes), Status::ok);
  bytes[24] = 1u;   /* a tag */
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed);
  bytes[24] = 0u;
  bytes[12] = 3u;   /* not ok */
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed);
  bytes[12] = 0u;
  bytes[36] = 1u;
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed);
  /* A reason's word in a release is reserved. */
  CHECK_STATUS(ExportAnswer(ExportRequest(ExportOperation::release, surface, tag), Status::ok).encode(bytes, sizeof bytes),
               Status::ok);
  bytes[32] = 1u;
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed); }

/* What answers a request that could not be read names nothing back. */
TEST(export_answer, an_unreadable_request_is_answered_with_nothing_named)
{ std::uint8_t bytes[ExportAnswer::encoded_size] = {};
  CHECK_STATUS(ExportAnswer().encode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(u32_at(bytes, 8u), 0u);
  CHECK_EQ(u32_at(bytes, 12u), 1u);
  CHECK_STATUS(ExportAnswer(Status::not_supported).encode(bytes, sizeof bytes), Status::ok);
  CHECK_EQ(u32_at(bytes, 12u), 2u);
  ExportAnswer read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::ok);
  CHECK(read.operation() == ExportOperation::unreadable);
  CHECK_STATUS(read.status(), Status::not_supported);
  CHECK_STATUS(ExportAnswer(Status::ok).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(ExportAnswer(Status::not_found).encode(bytes, sizeof bytes), Status::invalid_parameter);
  CHECK_STATUS(ExportAnswer().encode(bytes, sizeof bytes), Status::ok);
  bytes[24] = 1u;   /* a tag */
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed);
  bytes[24] = 0u;
  bytes[16] = 1u;   /* a surface */
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed); }

TEST(export_answer, another_major_length_magic_or_operation_is_refused)
{ std::uint8_t bytes[ExportAnswer::encoded_size + 1u] = {};
  CHECK_STATUS(exported().encode(bytes, sizeof bytes), Status::ok);
  ExportAnswer read;
  CHECK_STATUS(read.decode(bytes, ExportAnswer::encoded_size + 1u, 1u), Status::malformed);
  CHECK_STATUS(read.decode(bytes, ExportAnswer::encoded_size - 1u, 1u), Status::malformed);
  bytes[4] = 2u;
  CHECK_STATUS(read.decode(bytes, ExportAnswer::encoded_size, 1u), Status::not_supported);
  bytes[4] = 1u;
  bytes[6] = 9u;   /* another minor is this layout */
  CHECK_STATUS(read.decode(bytes, ExportAnswer::encoded_size, 1u), Status::ok);
  bytes[0] = 0u;
  CHECK_STATUS(read.decode(bytes, ExportAnswer::encoded_size, 1u), Status::malformed);
  CHECK_STATUS(ExportAnswer(ExportRequest(ExportOperation::release, surface, tag), Status::ok).encode(bytes, sizeof bytes),
               Status::ok);
  bytes[8] = 6u;
  CHECK_STATUS(read.decode(bytes, ExportAnswer::encoded_size, 0u), Status::malformed);
  CHECK_EQ(read.layout().width(), 1003u); }

/* A timing names no surface and carries nothing back: ok, or unavailable from
 * a device with no display clock yet. */
TEST(export_answer, a_timing_is_answered_with_nothing_named_and_no_body)
{ const Status statuses[] = {Status::ok, Status::invalid_state};
  for (const Status status : statuses) {
      std::uint8_t       bytes[ExportAnswer::encoded_size] = {};
      const ExportAnswer sent(ExportRequest(144000u, 5u, tag), status);
      CHECK_EQ(sent.surface(), 0u);
      CHECK_EQ(sent.descriptors(), 0u);
      CHECK_STATUS(sent.encode(bytes, sizeof bytes), Status::ok);
      CHECK_EQ(u32_at(bytes, 8u), 5u);
      for (std::size_t at = 16u; at < 24u; at++) CHECK_EQ(bytes[at], 0u);
      ExportAnswer read;
      CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::ok);
      CHECK(read.operation() == ExportOperation::timing);
      CHECK_STATUS(read.status(), status);
      CHECK_EQ(read.tag(), tag);
      bytes[16] = 1u;   /* naming a surface */
      CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed);
      bytes[16] = 0u;
      bytes[32] = 1u;   /* carrying a body */
      CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed);
  }
  ExportAnswer counted(ExportRequest(144000u, 5u, tag), Status::ok);
  counted.count(1u);
  std::uint8_t bytes[ExportAnswer::encoded_size] = {};
  CHECK_STATUS(counted.encode(bytes, sizeof bytes), Status::invalid_parameter); }

/* Each answer carries its operation's body and no other's: a greeting, a
 * layout or a count given to an answer that does not carry it is refused at
 * encode, so the device cannot write what a consumer would refuse. */
TEST(export_answer, a_body_its_operation_does_not_carry_is_refused_at_encode)
{ const ExportGreeting greeting(uuid);
  const ExportedLayout layout(64u, 64u, 0x34325241u, 256u, 0u, 0u);
  const ExportAnswer   answers[] = {
      ExportAnswer(ExportRequest(ExportOperation::hello, 0u, tag), Status::ok),
      ExportAnswer(ExportRequest(ExportOperation::export_surface, surface, tag), Status::ok),
      ExportAnswer(ExportRequest(ExportOperation::sync, surface, tag), Status::ok),
      ExportAnswer(ExportRequest(ExportOperation::release, surface, tag), Status::ok),
      ExportAnswer(ExportRevocation::released, surface),
      ExportAnswer(),
  };
  for (const ExportAnswer &answer : answers) {
      const bool hello = answer.operation() == ExportOperation::hello;
      const bool exports = answer.operation() == ExportOperation::export_surface;
      const bool syncs = answer.operation() == ExportOperation::sync;
      std::uint8_t bytes[ExportAnswer::encoded_size] = {};
      ExportAnswer greeted = answer;
      greeted.greet(greeting);
      if (exports) greeted.describe(layout);
      CHECK_STATUS(greeted.encode(bytes, sizeof bytes), hello ? Status::ok : Status::invalid_parameter);
      ExportAnswer described = answer;
      described.describe(layout);
      if (hello) described.greet(greeting);
      CHECK_STATUS(described.encode(bytes, sizeof bytes), exports ? Status::ok : Status::invalid_parameter);
      ExportAnswer counted = answer;
      counted.count(1u);
      if (hello) counted.greet(greeting);
      if (exports) counted.describe(layout);
      CHECK_STATUS(counted.encode(bytes, sizeof bytes), syncs ? Status::ok : Status::invalid_parameter);
  } }

/* Hello names no surface; the answers to the other three name one. */
TEST(export_answer, an_answer_naming_the_surface_its_operation_does_not_is_malformed)
{ std::uint8_t bytes[ExportAnswer::encoded_size] = {};
  ExportAnswer greeted(ExportRequest(ExportOperation::hello, 0u, tag), Status::ok);
  greeted.greet(ExportGreeting(uuid));
  CHECK_STATUS(greeted.encode(bytes, sizeof bytes), Status::ok);
  bytes[16] = 1u;
  ExportAnswer read;
  CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed);
  const ExportOperation named[] = {ExportOperation::sync, ExportOperation::release};
  for (const ExportOperation operation : named) {
      CHECK_STATUS(ExportAnswer(ExportRequest(operation, surface, tag), Status::not_found).encode(bytes, sizeof bytes),
                   Status::ok);
      std::memset(bytes + 16u, 0, 8u);
      CHECK_STATUS(read.decode(bytes, sizeof bytes, 0u), Status::malformed);
  } }

TEST(export_answer_decode, survives_the_hostile_sweep)
{ std::uint8_t bytes[ExportAnswer::encoded_size] = {};
  CHECK_STATUS(exported().encode(bytes, sizeof bytes), Status::ok);
  const av::HostileSweep sweep(rejects_export, bytes, sizeof bytes);
  sweep.run();
  CHECK_EQ(sweep.cases_run(), 64u + 1u + 8u * 64u + 2u);
  /* Free: the minor's 16 flips; the surface's 64 (no single flip zeroes an
   * id with more than one bit set) and the tag's 64; height's 32 (never 0
   * from 701); offset's 32; the modifier's 64; the fourcc's 32 but none
   * that zero it; the stride's that keep it at least 1003 wide: 4096 has
   * one bit, whose flip is 0, so 31; the width's that keep it at most
   * 4096 and not 0: of 1003's 32, the 20 above bit 11 fail, 12 pass; and
   * the operation's bit 0 makes export a sync, whose body is not empty: 0;
   * its bit 2 makes it a head, whose answer is laid out as an export's: 1.
   * With the untouched answer, 349. */
  const std::uint32_t acceptable = 16u + 64u + 64u + 32u + 32u + 64u + 32u + 31u + 12u + 1u + 1u;
  CHECK(sweep.rejections() >= sweep.cases_run() - acceptable); }
