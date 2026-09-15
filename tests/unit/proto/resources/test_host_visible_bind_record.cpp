/* The escape record a process sends to bind a host-visible chunk to its rvk
 * context, or unbind one (feature 045, contracts/host_visible_memory.md §1).
 * The kernel decodes it from memory the process wrote, so every field is judged
 * as hostile input, and what the kernel answers is encoded back into the same
 * 32 bytes. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/host_visible_bind_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::HostVisibleBindRecord;
using av::Status;
using Operation = av::HostVisibleBindRecord::Operation;

namespace {

constexpr std::uint32_t chunk = 0x40000123u;
constexpr std::uint32_t resource = 0x00100045u;

class Bytes {
public:
    explicit Bytes(const HostVisibleBindRecord &record) { status_ = record.encode(bytes_, sizeof bytes_); }
    std::uint8_t *data() { return bytes_; }
    Status        status() const { return status_; }
    void          put_u32(std::size_t at, std::uint32_t value) { std::memcpy(bytes_ + at, &value, sizeof value); }
    void          put_u16(std::size_t at, std::uint16_t value) { std::memcpy(bytes_ + at, &value, sizeof value); }
    void          put_u64(std::size_t at, std::uint64_t value) { std::memcpy(bytes_ + at, &value, sizeof value); }

private:
    std::uint8_t bytes_[HostVisibleBindRecord::encoded_size] = {};
    Status       status_ = Status::ok;
};

Status decode(Bytes &bytes, HostVisibleBindRecord &record)
{
    return record.decode(bytes.data(), HostVisibleBindRecord::encoded_size);
}

}  // namespace

TEST(host_visible_bind_record, a_bind_request_round_trips_and_says_what_it_asks)
{ Bytes bytes(HostVisibleBindRecord(Operation::bind, chunk, 0u));
  CHECK_STATUS(bytes.status(), Status::ok);
  /* The layout the contract fixes, so the ICD and the KMD agree byte for byte. */
  const std::uint8_t leading[] = {0x52, 0x56, 0x4B, 0x48, 1u, 0u, 1u, 0u, 0x23, 0x01, 0x00, 0x40};
  CHECK(std::memcmp(bytes.data(), leading, sizeof leading) == 0);
  HostVisibleBindRecord decoded;
  CHECK_STATUS(decode(bytes, decoded), Status::ok);
  CHECK(decoded.binds());
  CHECK_EQ(decoded.allocation(), chunk);
  CHECK_EQ(decoded.resource_id(), 0u);
  CHECK_EQ(decoded.pages(), 0u); }

TEST(host_visible_bind_record, an_unbind_request_names_the_resource)
{ Bytes bytes(HostVisibleBindRecord(Operation::unbind, 0u, resource));
  HostVisibleBindRecord decoded;
  CHECK_STATUS(decode(bytes, decoded), Status::ok);
  CHECK(!decoded.binds());
  CHECK_EQ(decoded.resource_id(), resource); }

/* What the kernel writes back: the request with the resource and page count
 * filled in. It is an answer, not a request, so the kernel's decoder refuses it -
 * a process cannot pre-fill the fields the kernel owns. */
TEST(host_visible_bind_record, an_answer_carries_the_resource_and_pages_and_is_no_request)
{ const HostVisibleBindRecord answer = HostVisibleBindRecord(Operation::bind, chunk, 0u).answered(resource, 16384u);
  CHECK_EQ(answer.resource_id(), resource);
  CHECK_EQ(answer.pages(), 16384u);
  Bytes bytes(answer);
  CHECK_STATUS(bytes.status(), Status::ok);
  std::uint32_t written_resource = 0u;
  std::uint64_t written_pages = 0u;
  std::memcpy(&written_resource, bytes.data() + 12u, sizeof written_resource);
  std::memcpy(&written_pages, bytes.data() + 16u, sizeof written_pages);
  CHECK_EQ(written_resource, resource);
  CHECK_EQ(written_pages, 16384u);
  HostVisibleBindRecord decoded;
  CHECK_STATUS(decode(bytes, decoded), Status::malformed); }

TEST(host_visible_bind_record, every_field_the_contract_fixes_is_judged)
{ struct Case { const char *what; std::size_t at; std::uint8_t width; std::uint64_t value; };
  const Case cases[] = {
      {"magic", 0u, 4u, 0x544B5652u},        /* RVKT, the context record's */
      {"version 0", 4u, 2u, 0u},
      {"version 2", 4u, 2u, 2u},
      {"operation 0", 6u, 2u, 0u},
      {"operation 3", 6u, 2u, 3u},
      {"bind of allocation 0", 8u, 4u, 0u},
      {"bind with a resource already", 12u, 4u, resource},
      {"bind with pages already", 16u, 8u, 1u},
      {"reserved", 24u, 8u, 1u},
      {"reserved high bit", 24u, 8u, 0x8000000000000000u},
  };
  for (const Case &one : cases) {
      Bytes bytes(HostVisibleBindRecord(Operation::bind, chunk, 0u));
      if (one.width == 2u) bytes.put_u16(one.at, static_cast<std::uint16_t>(one.value));
      if (one.width == 4u) bytes.put_u32(one.at, static_cast<std::uint32_t>(one.value));
      if (one.width == 8u) bytes.put_u64(one.at, one.value);
      HostVisibleBindRecord decoded = HostVisibleBindRecord(Operation::unbind, 0u, resource);
      CHECK_STATUS(decode(bytes, decoded), Status::malformed);
      /* A refusal leaves the record as it was. */
      CHECK(!decoded.binds());
      CHECK_EQ(decoded.resource_id(), resource);
  }
  Bytes unbind_zero(HostVisibleBindRecord(Operation::unbind, 0u, resource));
  unbind_zero.put_u32(12u, 0u);
  HostVisibleBindRecord decoded;
  CHECK_STATUS(decode(unbind_zero, decoded), Status::malformed);
  Bytes unbind_pages(HostVisibleBindRecord(Operation::unbind, 0u, resource));
  unbind_pages.put_u64(16u, 4u);
  CHECK_STATUS(decode(unbind_pages, decoded), Status::malformed); }

TEST(host_visible_bind_record, a_short_buffer_is_refused_at_every_length_and_encode_refuses_one)
{ Bytes bytes(HostVisibleBindRecord(Operation::bind, chunk, 0u));
  for (std::size_t length = 0u; length < HostVisibleBindRecord::encoded_size; length++) {
      HostVisibleBindRecord decoded;
      CHECK(decoded.decode(bytes.data(), length) != Status::ok);
  }
  HostVisibleBindRecord decoded;
  CHECK(decoded.decode(nullptr, HostVisibleBindRecord::encoded_size) != Status::ok);
  std::uint8_t small[HostVisibleBindRecord::encoded_size - 1u] = {};
  CHECK(HostVisibleBindRecord(Operation::bind, chunk, 0u).encode(small, sizeof small) != Status::ok); }

/* One bit of a valid request flipped anywhere: either still a well-formed
 * request (it then says exactly what the flipped bytes say) or refused. */
TEST(host_visible_bind_record, a_byte_flip_sweep_decodes_only_what_the_bytes_say)
{ for (std::size_t byte = 0u; byte < HostVisibleBindRecord::encoded_size; byte++) {
      for (unsigned bit = 0u; bit < 8u; bit++) {
          Bytes bytes(HostVisibleBindRecord(Operation::bind, chunk, 0u));
          bytes.data()[byte] = static_cast<std::uint8_t>(bytes.data()[byte] ^ (1u << bit));
          HostVisibleBindRecord decoded;
          if (decoded.decode(bytes.data(), HostVisibleBindRecord::encoded_size) != Status::ok) continue;
          /* Only the allocation field can change and stay a request. */
          CHECK(byte >= 8u && byte < 12u);
          std::uint32_t allocation = 0u;
          std::memcpy(&allocation, bytes.data() + 8u, sizeof allocation);
          CHECK_EQ(decoded.allocation(), allocation);
      }
  } }
