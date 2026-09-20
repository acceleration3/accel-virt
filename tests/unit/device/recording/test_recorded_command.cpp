#include "core/support/status.hpp"
#include "device/recording/recorded_command.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/command_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::CommandHeader;
using av::RecordedCommand;
using av::Status;

namespace {

/* A batch payload of framed commands, as a guest writes one. */
class Batch {
public:
    Batch() : bytes_{}, length_(0u) {}

    Batch &add(std::uint32_t id, std::uint32_t payload, std::uint8_t flags = 0u)
    {
        const std::uint32_t size = CommandHeader::encoded_size + payload;
        CHECK_STATUS(CommandHeader(id, size, flags).encode(bytes_ + length_, sizeof bytes_ - length_),
                     Status::ok);
        for (std::uint32_t index = 0u; index < payload; index++)
            bytes_[length_ + CommandHeader::encoded_size + index] = static_cast<std::uint8_t>(index);
        length_ += size;
        return *this;
    }

    std::uint8_t  *bytes() { return bytes_; }
    std::uint32_t  length() const { return length_; }

private:
    std::uint8_t  bytes_[256];
    std::uint32_t length_;
};

}  // namespace

TEST(recorded_command_decode, names_each_api_from_the_table_its_generator_wrote)
{ Batch batch;
  batch.add(0x00010006u, 16u)            /* vkCreateInstance */
       .add(0x00020001u, 4u)             /* an OpenCL id */
       .add(0x00030001u, 0u)             /* an OpenGL id */
       .add(CommandHeader::nop_id, 0u);
  RecordedCommand command;
  CHECK_STATUS(command.decode(batch.bytes(), batch.length(), 0u), Status::ok);
  CHECK_EQ(std::strcmp(command.name(), "vkCreateInstance"), 0);
  CHECK_EQ(std::strcmp(command.api_name(), "vulkan"), 0);
  CHECK_EQ(command.payload_length(), 16u);
  CHECK_EQ(command.offset(), 0u);
  CHECK_EQ(command.next_offset(), CommandHeader::encoded_size + 16u);
  CHECK_EQ(command.payload()[3], 3u);

  CHECK_STATUS(command.decode(batch.bytes(), batch.length(), command.next_offset()), Status::ok);
  CHECK_EQ(std::strcmp(command.api_name(), "opencl"), 0);
  CHECK_EQ(std::strcmp(command.name(), "clGetPlatformInfo"), 0);

  CHECK_STATUS(command.decode(batch.bytes(), batch.length(), command.next_offset()), Status::ok);
  CHECK_EQ(std::strcmp(command.api_name(), "opengl"), 0);
  CHECK_EQ(std::strcmp(command.name(), "glGetStringi"), 0);

  CHECK_STATUS(command.decode(batch.bytes(), batch.length(), command.next_offset()), Status::ok);
  CHECK_EQ(std::strcmp(command.api_name(), "transport"), 0);
  CHECK_EQ(std::strcmp(command.name(), "nop"), 0);
  CHECK_EQ(command.next_offset(), batch.length()); }

TEST(recorded_command_decode, names_the_transport_commands_this_repository_owns)
{ struct Case { std::uint32_t id; const char *name; };
  const Case cases[] = {{CommandHeader::nop_id, "nop"},
                        {CommandHeader::import_blob_id, "import_blob"},
                        {CommandHeader::release_blob_id, "release_blob"},
                        {0x00000009u, "unknown"}};
  for (const Case &c : cases) {
      Batch batch;
      batch.add(c.id, 0u);
      RecordedCommand command;
      CHECK_STATUS(command.decode(batch.bytes(), batch.length(), 0u), Status::ok);
      CHECK_EQ(std::strcmp(command.name(), c.name), 0);
  } }

TEST(recorded_command_decode, an_id_no_table_knows_is_unknown_and_never_null)
{ Batch batch;
  batch.add(0x0001FFFEu, 0u).add(0x0002FFFEu, 0u).add(0x0003FFFEu, 0u).add(0x00090001u, 0u);
  RecordedCommand command;
  std::uint32_t   offset = 0u;
  /* Each table's own unknown, then an API no table belongs to. */
  const char *const apis[] = {"vulkan", "opencl", "opengl", "api"};
  for (const char *api : apis) {
      CHECK_STATUS(command.decode(batch.bytes(), batch.length(), offset), Status::ok);
      CHECK_EQ(std::strcmp(command.name(), "unknown"), 0);
      CHECK_EQ(std::strcmp(command.api_name(), api), 0);
      offset = command.next_offset();
  }
  CHECK_EQ(offset, batch.length()); }

TEST(recorded_command_decode, refuses_a_command_the_batch_cannot_hold)
{ Batch batch;
  batch.add(0x00010006u, 8u);
  RecordedCommand command;
  /* Nothing at all, a header cut short, an offset past the batch, and a size
   * that passes the end. */
  CHECK_STATUS(command.decode(nullptr, batch.length(), 0u), Status::malformed);
  for (std::uint32_t length = 0u; length < CommandHeader::encoded_size; length++)
      CHECK_STATUS(command.decode(batch.bytes(), length, 0u), Status::malformed);
  CHECK_STATUS(command.decode(batch.bytes(), batch.length(), batch.length() + 1u), Status::malformed);
  CHECK_STATUS(command.decode(batch.bytes(), batch.length(), batch.length()), Status::malformed);
  CHECK_STATUS(command.decode(batch.bytes(), batch.length() - 1u, 0u), Status::malformed); }

/* The bound is what is left from the offset, not the whole batch: the printer
 * and the replay walk a batch command by command, and a second command that
 * claims more than the rest of the record would be read past its end. */
TEST(recorded_command_decode, refuses_a_later_command_that_claims_more_than_is_left)
{ Batch batch;
  batch.add(0x00010006u, 4u).add(0x00010007u, 4u);
  RecordedCommand     command;
  const std::uint32_t second = CommandHeader::encoded_size + 4u;
  const std::uint32_t left = batch.length() - second;
  CHECK_STATUS(command.decode(batch.bytes(), batch.length(), second), Status::ok);
  CHECK_EQ(command.next_offset(), batch.length());
  /* One byte more than the rest of the batch holds, and less than the batch. */
  CHECK_STATUS(CommandHeader(0x00010007u, left + 1u, 0u).encode(batch.bytes() + second, left), Status::ok);
  CHECK_STATUS(command.decode(batch.bytes(), batch.length(), second), Status::malformed);
  /* And a size the whole batch could hold is still past what is left here. */
  CHECK_STATUS(CommandHeader(0x00010007u, batch.length(), 0u).encode(batch.bytes() + second, left), Status::ok);
  CHECK_STATUS(command.decode(batch.bytes(), batch.length(), second), Status::malformed); }

/* Every accepted command's payload lies inside the bytes it was given. */
TEST(recorded_command_decode, an_accepted_command_never_names_a_byte_past_the_batch)
{ Batch batch;
  batch.add(0x00010006u, 16u).add(0x00020001u, 0u).add(CommandHeader::nop_id, 7u);
  RecordedCommand command;
  std::uint32_t   offset = 0u;
  while (offset != batch.length()) {
      CHECK_STATUS(command.decode(batch.bytes(), batch.length(), offset), Status::ok);
      CHECK(command.payload() >= batch.bytes() + offset + CommandHeader::encoded_size);
      CHECK(command.payload() + command.payload_length() <= batch.bytes() + batch.length());
      CHECK_EQ(command.next_offset(), offset + CommandHeader::encoded_size + command.payload_length());
      offset = command.next_offset();
  } }

TEST(recorded_command_decode, leaves_what_it_had_when_it_refuses)
{ Batch batch;
  batch.add(0x00010006u, 8u);
  RecordedCommand command;
  CHECK_STATUS(command.decode(batch.bytes(), batch.length(), 0u), Status::ok);
  const std::uint8_t *payload = command.payload();
  CHECK_STATUS(command.decode(batch.bytes(), 4u, 0u), Status::malformed);
  CHECK_EQ(command.header().id(), 0x00010006u);
  CHECK(command.payload() == payload);
  CHECK_EQ(command.payload_length(), 8u); }
