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
  CHECK(command.name()[0] != '\0');

  CHECK_STATUS(command.decode(batch.bytes(), batch.length(), command.next_offset()), Status::ok);
  CHECK_EQ(std::strcmp(command.api_name(), "opengl"), 0);
  CHECK(command.name()[0] != '\0');

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
  batch.add(0x0001FFFEu, 0u).add(0x00090001u, 0u);
  RecordedCommand command;
  CHECK_STATUS(command.decode(batch.bytes(), batch.length(), 0u), Status::ok);
  CHECK_EQ(std::strcmp(command.name(), "unknown"), 0);
  CHECK_STATUS(command.decode(batch.bytes(), batch.length(), command.next_offset()), Status::ok);
  CHECK_EQ(std::strcmp(command.name(), "unknown"), 0);
  CHECK_EQ(std::strcmp(command.api_name(), "api"), 0); }

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
