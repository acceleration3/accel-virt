/* The escape record a process sends to take a chunk of its device's host
 * window, or give one back (docs/host-memory.md). The kernel decodes it from
 * memory the process wrote, so every field is judged as hostile input, and what
 * the kernel answers is encoded back into the same 32 bytes. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/escape/host_window_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::HostMemoryRequest;
using av::HostWindowRecord;
using av::Status;
using Operation = av::HostWindowRecord::Operation;

namespace {

/* Several bits set, so no single flipped bit can empty it. */
constexpr std::uint64_t asked = 0x123800u;
constexpr std::uint64_t granted = 0x124000u;
constexpr std::uint32_t resource = 0x00100045u;
constexpr std::uint64_t address = 0x00007FF000010000ull;
/* A Vulkan type, so the kind and the type are both on the wire. */
const HostMemoryRequest memory(HostMemoryRequest::Kind::vulkan, 5u);
const HostMemoryRequest none;

class Bytes {
public:
    explicit Bytes(const HostWindowRecord &record) { status_ = record.encode(bytes_, sizeof bytes_); }
    std::uint8_t *data() { return bytes_; }
    Status        status() const { return status_; }
    void          put(std::size_t at, std::uint8_t width, std::uint64_t value)
    { std::memcpy(bytes_ + at, &value, width); }

private:
    std::uint8_t bytes_[HostWindowRecord::encoded_size] = {};
    Status       status_ = Status::ok;
};

Status decode(Bytes &bytes, HostWindowRecord &record)
{
    return record.decode(bytes.data(), HostWindowRecord::encoded_size);
}

std::uint64_t u64_at(const std::uint8_t *bytes, std::size_t at)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

}  // namespace

TEST(host_window_record, an_open_round_trips_and_says_what_it_asks)
{ Bytes bytes(HostWindowRecord(Operation::open, asked, 0u, memory));
  CHECK_STATUS(bytes.status(), Status::ok);
  /* The layout the guest and the kernel agree on byte for byte: "AVW",
   * version 2, open, no resource, the memory, the bytes, no address. */
  const std::uint8_t leading[] = {0x41, 0x56, 0x57, 0x00, 2u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 3u, 5u, 0u, 0u};
  CHECK(std::memcmp(bytes.data(), leading, sizeof leading) == 0);
  CHECK_EQ(u64_at(bytes.data(), 16u), asked);
  CHECK_EQ(u64_at(bytes.data(), 24u), 0u);
  HostWindowRecord decoded;
  CHECK_STATUS(decode(bytes, decoded), Status::ok);
  CHECK(decoded.opens());
  CHECK_EQ(decoded.bytes(), asked);
  CHECK_EQ(decoded.resource_id(), 0u);
  CHECK_EQ(decoded.memory().encoded(), memory.encoded());
  CHECK_EQ(decoded.address(), 0u); }

TEST(host_window_record, a_close_names_the_resource)
{ Bytes bytes(HostWindowRecord(Operation::close, 0u, resource, none));
  HostWindowRecord decoded;
  CHECK_STATUS(decode(bytes, decoded), Status::ok);
  CHECK(!decoded.opens());
  CHECK_EQ(decoded.resource_id(), resource);
  CHECK_EQ(decoded.memory().encoded(), 0u);
  CHECK_EQ(decoded.bytes(), 0u); }

/* What the kernel writes back: the open with the resource, the bytes granted
 * and the address filled in. It is an answer, not a request, so the kernel's
 * decoder refuses it - a process cannot pre-fill the fields the kernel owns. */
TEST(host_window_record, an_answer_carries_the_resource_bytes_and_address_and_is_no_request)
{ const HostWindowRecord answer = HostWindowRecord(Operation::open, asked, 0u, memory).answered(resource, granted, address);
  CHECK(answer.opens());
  CHECK_EQ(answer.resource_id(), resource);
  CHECK_EQ(answer.bytes(), granted);
  CHECK_EQ(answer.address(), address);
  Bytes bytes(answer);
  CHECK_STATUS(bytes.status(), Status::ok);
  std::uint32_t written_resource = 0u;
  std::memcpy(&written_resource, bytes.data() + 8u, sizeof written_resource);
  CHECK_EQ(written_resource, resource);
  CHECK_EQ(u64_at(bytes.data(), 16u), granted);
  CHECK_EQ(u64_at(bytes.data(), 24u), address);
  HostWindowRecord decoded;
  CHECK_STATUS(decode(bytes, decoded), Status::malformed); }

TEST(host_window_record, every_field_is_judged)
{ struct Case { const char *what; Operation operation; std::size_t at; std::uint8_t width; std::uint64_t value; };
  const Case cases[] = {
      {"magic", Operation::open, 0u, 4u, 0x00485641u},          /* AVH, the chunk binding's */
      {"version 0", Operation::open, 4u, 2u, 0u},
      {"version 1", Operation::open, 4u, 2u, 1u},
      {"version 3", Operation::open, 4u, 2u, 3u},
      {"operation 0", Operation::open, 6u, 2u, 0u},
      {"operation 3", Operation::open, 6u, 2u, 3u},
      {"open with a resource already", Operation::open, 8u, 4u, resource},
      {"open of no memory", Operation::open, 12u, 4u, 0u},
      {"open of a memory kind there is none of", Operation::open, 12u, 4u, 4u},
      {"open of a Vulkan type past the last", Operation::open, 12u, 4u, 0x2003u},
      {"open of a class naming a type", Operation::open, 12u, 4u, 0x0101u},
      {"open of no bytes", Operation::open, 16u, 8u, 0u},
      {"open with an address already", Operation::open, 24u, 8u, address},
      {"close of no resource", Operation::close, 8u, 4u, 0u},
      {"close with bytes", Operation::close, 16u, 8u, 1u},
      {"close with an address", Operation::close, 24u, 8u, 1u},
      {"close naming memory", Operation::close, 12u, 4u, 1u},
      {"close memory high bit", Operation::close, 12u, 4u, 0x80000000u},
  };
  for (const Case &one : cases) {
      const bool opening = one.operation == Operation::open;
      Bytes bytes(HostWindowRecord(one.operation, opening ? asked : 0u, opening ? 0u : resource, opening ? memory : none));
      bytes.put(one.at, one.width, one.value);
      HostWindowRecord decoded(Operation::close, 0u, 7u, none);
      CHECK_STATUS(decode(bytes, decoded), Status::malformed);
      /* A refusal leaves the record as it was. */
      CHECK(!decoded.opens());
      CHECK_EQ(decoded.resource_id(), 7u);
  } }

TEST(host_window_record, a_short_buffer_is_refused_at_every_length_and_encode_refuses_one)
{ Bytes bytes(HostWindowRecord(Operation::open, asked, 0u, memory));
  for (std::size_t length = 0u; length < HostWindowRecord::encoded_size; length++) {
      HostWindowRecord decoded;
      CHECK(decoded.decode(bytes.data(), length) != Status::ok);
  }
  HostWindowRecord decoded;
  CHECK(decoded.decode(bytes.data(), HostWindowRecord::encoded_size + 1u) != Status::ok);
  CHECK(decoded.decode(nullptr, HostWindowRecord::encoded_size) != Status::ok);
  std::uint8_t small[HostWindowRecord::encoded_size - 1u] = {};
  CHECK(HostWindowRecord(Operation::open, asked, 0u, memory).encode(small, sizeof small) != Status::ok); }

/* One bit of a valid open flipped anywhere: either still a well-formed request
 * (it then says exactly what the flipped bytes say) or refused. */
TEST(host_window_record, a_byte_flip_sweep_decodes_only_what_the_bytes_say)
{ for (std::size_t byte = 0u; byte < HostWindowRecord::encoded_size; byte++) {
      for (unsigned bit = 0u; bit < 8u; bit++) {
          Bytes bytes(HostWindowRecord(Operation::open, asked, 0u, memory));
          bytes.data()[byte] = static_cast<std::uint8_t>(bytes.data()[byte] ^ (1u << bit));
          HostWindowRecord decoded;
          if (decoded.decode(bytes.data(), HostWindowRecord::encoded_size) != Status::ok) continue;
          /* Only the bytes asked for, or the Vulkan type to another type
           * below 32, can change and stay a request. */
          CHECK((byte >= 16u && byte < 24u) || byte == 13u);
          CHECK_EQ(decoded.bytes(), u64_at(bytes.data(), 16u));
          CHECK_EQ(decoded.memory().encoded(), std::uint32_t{bytes.data()[12]} | std::uint32_t{bytes.data()[13]} << 8u);
      }
  } }

/* The process reads the kernel's answer back from the bytes it sent: an open
 * with a resource, at least the bytes asked for, and an address; a close is
 * answered with itself. */
TEST(host_window_record, accept_answer_takes_the_resource_bytes_and_address_the_kernel_wrote)
{ HostWindowRecord request(Operation::open, asked, 0u, memory);
  Bytes bytes(request.answered(resource, granted, address));
  CHECK_STATUS(request.accept_answer(bytes.data(), HostWindowRecord::encoded_size), Status::ok);
  CHECK(request.opens());
  CHECK_EQ(request.resource_id(), resource);
  CHECK_EQ(request.bytes(), granted);
  CHECK_EQ(request.address(), address);
  /* Exactly the bytes asked for is enough, when they were whole pages. */
  HostWindowRecord whole(Operation::open, granted, 0u, memory);
  Bytes exact(whole.answered(resource, granted, address));
  CHECK_STATUS(whole.accept_answer(exact.data(), HostWindowRecord::encoded_size), Status::ok);
  HostWindowRecord close(Operation::close, 0u, resource, none);
  Bytes echoed(close);
  CHECK_STATUS(close.accept_answer(echoed.data(), HostWindowRecord::encoded_size), Status::ok);
  CHECK_EQ(close.resource_id(), resource);
  CHECK_EQ(close.bytes(), 0u);
  CHECK_EQ(close.address(), 0u); }

/* An answer that does not answer this request is refused and the record is
 * left as it was. */
TEST(host_window_record, accept_answer_refuses_what_does_not_answer_the_request)
{ struct Case { const char *what; Operation operation; std::size_t at; std::uint8_t width; std::uint64_t value; };
  const Case cases[] = {
      {"magic", Operation::open, 0u, 4u, 0x00485641u},
      {"version", Operation::open, 4u, 2u, 1u},
      {"operation", Operation::open, 6u, 2u, 2u},
      {"no resource", Operation::open, 8u, 4u, 0u},
      {"other memory", Operation::open, 12u, 4u, 0x0603u},
      {"no memory", Operation::open, 12u, 4u, 0u},
      {"fewer bytes than asked", Operation::open, 16u, 8u, asked - 1u},
      {"no address", Operation::open, 24u, 8u, 0u},
      {"close of another resource", Operation::close, 8u, 4u, resource + 1u},
      {"close with bytes", Operation::close, 16u, 8u, 1u},
      {"close with an address", Operation::close, 24u, 8u, address},
      {"close turned open", Operation::close, 6u, 2u, 1u},
      {"close with memory", Operation::close, 12u, 4u, 1u},
  };
  for (const Case &one : cases) {
      const bool opening = one.operation == Operation::open;
      HostWindowRecord request(one.operation, opening ? asked : 0u, opening ? 0u : resource, opening ? memory : none);
      Bytes bytes(opening ? request.answered(resource, granted, address) : request);
      bytes.put(one.at, one.width, one.value);
      CHECK_STATUS(request.accept_answer(bytes.data(), HostWindowRecord::encoded_size), Status::malformed);
      CHECK_EQ(request.resource_id(), opening ? 0u : resource);
      CHECK_EQ(request.bytes(), opening ? asked : 0u);
      CHECK_EQ(request.address(), 0u);
  }
  /* A record that is no request has no answer, even to its own bytes. */
  HostWindowRecord nothing;
  Bytes own(nothing);
  CHECK_STATUS(nothing.accept_answer(own.data(), HostWindowRecord::encoded_size), Status::malformed);
  HostWindowRecord request(Operation::open, asked, 0u, memory);
  Bytes answer(request.answered(resource, granted, address));
  for (std::size_t length = 0u; length < HostWindowRecord::encoded_size; length++)
      CHECK(request.accept_answer(answer.data(), length) != Status::ok);
  CHECK(request.accept_answer(nullptr, HostWindowRecord::encoded_size) != Status::ok);
  CHECK_EQ(request.resource_id(), 0u);
  CHECK_STATUS(request.accept_answer(answer.data(), HostWindowRecord::encoded_size), Status::ok);
  CHECK_EQ(request.address(), address);
  CHECK_EQ(request.memory().encoded(), memory.encoded()); }
