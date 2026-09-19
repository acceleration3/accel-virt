#include "core/support/status.hpp"
#include "device/recording/context_api.hpp"
#include "device/recording/recording_filter.hpp"
#include "device/recording/recording_header.hpp"
#include "device/recording/recording_limit.hpp"
#include "device/recording/recording_printer.hpp"
#include "device/recording/recording_reader.hpp"
#include "device/recording/wire_recorder.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/fault_code.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

using av::AtCeiling;
using av::BlobAction;
using av::CommandHeader;
using av::ContextApi;
using av::ContextSetup;
using av::FaultCode;
using av::GpuIdentity;
using av::MockHost;
using av::ProtocolVersion;
using av::RecordingFilter;
using av::RecordingHeader;
using av::RecordingPrinter;
using av::RecordingReader;
using av::Status;
using av::SubApi;
using av::WireRecorder;

namespace {

constexpr const char *path = "print.rvk";
constexpr std::size_t widest = 8192u;
constexpr std::size_t most_lines = 64u;

/* A batch payload of framed commands. */
class Batch {
public:
    Batch() : bytes_{}, length_(0u) {}

    Batch &add(std::uint32_t id, const std::uint8_t *payload, std::uint32_t payload_length,
               std::uint8_t flags = 0u)
    {
        const std::uint32_t size = CommandHeader::encoded_size + payload_length;
        CHECK_STATUS(CommandHeader(id, size, flags).encode(bytes_ + length_, sizeof bytes_ - length_),
                     Status::ok);
        for (std::uint32_t index = 0u; index < payload_length; index++)
            bytes_[length_ + CommandHeader::encoded_size + index] = payload[index];
        length_ += size;
        return *this;
    }

    const std::uint8_t *bytes() const { return bytes_; }
    std::uint32_t       length() const { return length_; }

private:
    std::uint8_t  bytes_[512];
    std::uint32_t length_;
};

/* A recording the device wrote, printed: the lines, in order. */
class Printed {
public:
    Printed(const std::uint8_t *bytes, std::size_t length, const RecordingFilter &filter,
            std::uint32_t payload_bytes)
        : reader_(bytes, length), printer_(reader_, filter, payload_bytes), count_(0u), lines_{}
    {
        const char *line = nullptr;
        while (printer_.next(line) == Status::ok) {
            CHECK(line != nullptr);
            CHECK(printer_.invariants_hold());
            if (count_ == most_lines) {
                av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "more lines than expected");
                return;
            }
            std::snprintf(lines_[count_], sizeof lines_[0], "%s", line);
            count_++;
        }
        CHECK_EQ(printer_.lines(), count_);
        CHECK(printer_.invariants_hold());
    }

    std::size_t count() const { return count_; }
    const char *line(std::size_t index) const { return index < count_ ? lines_[index] : ""; }
    bool        damaged() const { return printer_.damaged(); }
    /* Whether some line holds `fragment`. */
    bool        holds(const char *fragment) const
    {
        for (std::size_t index = 0u; index < count_; index++)
            if (std::strstr(lines_[index], fragment) != nullptr) return true;
        return false;
    }

private:
    RecordingReader  reader_;
    RecordingPrinter printer_;
    std::size_t      count_;
    char             lines_[most_lines][RecordingPrinter::line_bytes];
};

/* The recording every printer test reads: one Vulkan context's setup, a batch
 * of two commands with a reply, a fence, a transport batch that imports a
 * blob, a fault and a blob. Written by the device's own recorder, so a change
 * to either side that breaks the other fails here (T022). */
class Written {
public:
    Written() : host_(), recorder_(host_), length_(0u), bytes_{}, vulkan_(), transport_()
    {
        const std::uint8_t uuid[GpuIdentity::uuid_bytes] = {0x31u};
        CHECK_STATUS(recorder_.open(path, 0u, "all", AtCeiling::stop,
                                    GpuIdentity(1u, 2u, GpuIdentity::discrete_gpu, uuid, 0u, 0u, "mock"),
                                    ProtocolVersion(1u, 1u)),
                     Status::ok);
        const std::uint8_t body[16] = {0xC0u, 0xC1u, 0xC2u, 0xC3u};
        vulkan_.add(0x00010006u, body, 16u, CommandHeader::generate_reply)
            .add(0x00010007u, body, 4u);
        std::uint8_t import[32] = {};
        import[0] = 10u;               /* resource 10 */
        import[16] = 0x00u; import[17] = 0x10u;  /* size 0x1000 */
        transport_.add(CommandHeader::import_blob_id, import, sizeof import);

        recorder_.record_context_setup(3u, ContextSetup(ProtocolVersion(1u, 0u), SubApi::vulkan, 40u, 16u,
                                                        41u, 1u, 42u));
        recorder_.record_batch(3u, 1u, vulkan_.bytes(), vulkan_.length());
        const std::uint8_t answer[4] = {0xD0u, 0xD1u, 0xD2u, 0xD3u};
        recorder_.record_reply(3u, 1u, answer, sizeof answer);
        recorder_.record_fence(3u, 1u);
        recorder_.record_batch(3u, 2u, transport_.bytes(), transport_.length());
        recorder_.record_fault(3u, 2u, FaultCode::undecodable);
        recorder_.record_blob(3u, BlobAction::imported, 5u, 9u, 4096u, 8192u);
        recorder_.close();
        length_ = host_.recording_length(path);
        CHECK(length_ <= sizeof bytes_);
        if (length_ <= sizeof bytes_) std::memcpy(bytes_, host_.recording_bytes(path), length_);
    }

    const std::uint8_t *bytes() const { return bytes_; }
    std::size_t         length() const { return length_; }
    const Batch        &vulkan() const { return vulkan_; }

private:
    MockHost     host_;
    WireRecorder recorder_;
    std::size_t  length_;
    std::uint8_t bytes_[widest];
    Batch        vulkan_;
    Batch        transport_;
};

}  // namespace

TEST(recording_printer_next, prints_a_line_per_record_and_one_per_command)
{ const Written        written;
  const RecordingFilter filter;
  const Printed        printed(written.bytes(), written.length(), filter, 0u);
  CHECK(!printed.damaged());
  /* setup, batch + 2 commands, reply, fence, batch + 1 command, fault, blob. */
  CHECK_EQ(printed.count(), 10u);
  CHECK(std::strstr(printed.line(0u), "ctx=3 setup api=vulkan ring_order=16 doorbell=1 reply=42 version=1.0")
        != nullptr);
  CHECK(std::strstr(printed.line(1u), "fence=1 batch ") != nullptr);
  CHECK(std::strstr(printed.line(1u), " 2 commands") != nullptr);
  CHECK(std::strstr(printed.line(2u), "0x00010006 vkCreateInstance 16 bytes reply") != nullptr);
  CHECK(std::strstr(printed.line(3u), "0x00010007 vkCreateDevice 4 bytes") != nullptr);
  CHECK(std::strstr(printed.line(3u), "reply") == nullptr);
  CHECK(std::strstr(printed.line(4u), "fence=1 reply 4 bytes") != nullptr);
  CHECK(std::strstr(printed.line(5u), "fence=1 fence completed") != nullptr);
  CHECK(std::strstr(printed.line(6u), "fence=2 batch ") != nullptr);
  CHECK(std::strstr(printed.line(7u),
                    "0x00000002 import_blob 32 bytes resource=10 offset=0 size=4096 memory_type=0")
        != nullptr);
  CHECK(std::strstr(printed.line(8u), "fence=2 fault 6 undecodable") != nullptr);
  CHECK(std::strstr(printed.line(9u), "blob imported id=9 offset=4096 size=8192 memory_type=5")
        != nullptr); }

TEST(recording_printer_next, every_line_opens_with_its_offset_and_the_time_it_was_taken)
{ const Written        written;
  const RecordingFilter filter;
  const Printed        printed(written.bytes(), written.length(), filter, 0u);
  /* The first record starts right after the file header. */
  char opening[32] = {};
  std::snprintf(opening, sizeof opening, "%08X +", static_cast<unsigned>(RecordingHeader::encoded_size));
  CHECK_EQ(std::strncmp(printed.line(0u), opening, std::strlen(opening)), 0);
  /* A command's line is indented under the batch it belongs to. */
  CHECK_EQ(printed.line(2u)[0], ' ');
  CHECK_EQ(printed.line(1u)[0], '0'); }

TEST(recording_printer_next, shows_as_many_payload_bytes_as_it_was_asked_for)
{ const Written written;
  const RecordingFilter filter;
  CHECK(!Printed(written.bytes(), written.length(), filter, 0u).holds("c0c1c2c3"));
  CHECK(!Printed(written.bytes(), written.length(), filter, 0u).holds("C0C1C2C3"));
  CHECK(Printed(written.bytes(), written.length(), filter, 4u).holds("C0C1C2C3"));
  CHECK(!Printed(written.bytes(), written.length(), filter, 4u).holds("C0C1C2C300"));
  /* More than the payload holds shows the payload and stops. */
  CHECK(Printed(written.bytes(), written.length(), filter, 200u).holds("C0C1C2C300000000")); }

TEST(recording_printer_next, a_context_filter_keeps_only_that_contexts_records)
{ const Written written;
  RecordingFilter filter;
  filter.keep_context(9u);
  const Printed printed(written.bytes(), written.length(), filter, 0u);
  CHECK_EQ(printed.count(), 0u);
  RecordingFilter kept;
  kept.keep_context(3u);
  CHECK_EQ(Printed(written.bytes(), written.length(), kept, 0u).count(), 10u); }

TEST(recording_printer_next, a_fence_filter_keeps_the_batches_of_that_range)
{ const Written written;
  RecordingFilter filter;
  CHECK_STATUS(filter.keep_fences(2u, 2u), Status::ok);
  const Printed printed(written.bytes(), written.length(), filter, 0u);
  CHECK(!printed.holds("fence=1"));
  CHECK(printed.holds("fence=2 batch"));
  CHECK(printed.holds("fault 6"));
  /* The setup and the blob belong to no batch and stay. */
  CHECK(printed.holds("setup api=vulkan"));
  CHECK(printed.holds("blob imported")); }

TEST(recording_printer_next, a_command_filter_keeps_the_batches_that_hold_one)
{ const Written written;
  RecordingFilter filter;
  CHECK_STATUS(filter.keep_command("vkCreateDevice"), Status::ok);
  const Printed printed(written.bytes(), written.length(), filter, 0u);
  /* The batch that holds it, and only the command that matched. */
  CHECK_EQ(printed.count(), 2u);
  CHECK(printed.holds("fence=1 batch"));
  CHECK(printed.holds("vkCreateDevice"));
  CHECK(!printed.holds("vkCreateInstance"));
  CHECK(!printed.holds("setup"));
  CHECK(!printed.holds("import_blob")); }

TEST(recording_printer_next, an_api_filter_keeps_that_apis_batches_and_its_contexts_records)
{ const Written written;
  RecordingFilter vulkan;
  CHECK_STATUS(vulkan.keep_api("vulkan"), Status::ok);
  const Printed kept(written.bytes(), written.length(), vulkan, 0u);
  CHECK(kept.holds("setup api=vulkan"));
  CHECK(kept.holds("vkCreateInstance"));
  CHECK(kept.holds("fence=1 reply"));
  CHECK(!kept.holds("import_blob"));

  RecordingFilter opencl;
  CHECK_STATUS(opencl.keep_api("opencl"), Status::ok);
  const Printed none(written.bytes(), written.length(), opencl, 0u);
  CHECK_EQ(none.count(), 0u);

  RecordingFilter transport;
  CHECK_STATUS(transport.keep_api("transport"), Status::ok);
  const Printed only(written.bytes(), written.length(), transport, 0u);
  CHECK(only.holds("import_blob"));
  CHECK(!only.holds("vkCreateInstance"));
  CHECK(!only.holds("setup")); }

TEST(recording_printer_next, prints_up_to_the_damage_and_says_where_it_is)
{ const Written written;
  std::uint8_t  cut[widest] = {};
  std::memcpy(cut, written.bytes(), written.length());
  const RecordingFilter filter;
  /* Cut inside the second record: the first is printed whole, then the damage. */
  const Printed printed(cut, written.length() - 4u, filter, 0u);
  CHECK(printed.damaged());
  CHECK(printed.count() >= 2u);
  CHECK(std::strstr(printed.line(printed.count() - 1u), "damaged at ") != nullptr);
  CHECK(printed.holds("setup api=vulkan"));
  /* The damage line is the last, whatever follows it in the file. */
  for (std::size_t index = 0u; index + 1u < printed.count(); index++)
      CHECK(std::strstr(printed.line(index), "damaged at ") == nullptr); }

TEST(recording_printer_next, a_file_that_is_no_recording_prints_one_line_of_damage)
{ const std::uint8_t nothing[8] = {};
  const RecordingFilter filter;
  const Printed printed(nothing, sizeof nothing, filter, 0u);
  CHECK_EQ(printed.count(), 1u);
  CHECK(printed.damaged());
  CHECK(std::strstr(printed.line(0u), "damaged at 00000000: ") != nullptr); }

TEST(recording_printer_next, a_batch_whose_commands_are_framed_wrong_says_where_it_stopped)
{ MockHost     host;
  WireRecorder recorder(host);
  const std::uint8_t uuid[GpuIdentity::uuid_bytes] = {0x31u};
  CHECK_STATUS(recorder.open(path, 0u, "all", AtCeiling::stop,
                             GpuIdentity(1u, 2u, GpuIdentity::discrete_gpu, uuid, 0u, 0u, "mock"),
                             ProtocolVersion(1u, 1u)),
               Status::ok);
  Batch              batch;
  const std::uint8_t body[4] = {1u, 2u, 3u, 4u};
  batch.add(0x00010006u, body, 4u);
  /* A second command's header that claims more than the batch holds. */
  std::uint8_t bytes[64] = {};
  std::memcpy(bytes, batch.bytes(), batch.length());
  CHECK_STATUS(CommandHeader(0x00010007u, 200u, 0u).encode(bytes + batch.length(), 8u), Status::ok);
  recorder.record_batch(1u, 1u, bytes, batch.length() + 8u);
  recorder.close();

  const RecordingFilter filter;
  const Printed         printed(host.recording_bytes(path), host.recording_length(path), filter, 0u);
  CHECK(!printed.damaged());
  CHECK_EQ(printed.count(), 3u);
  CHECK(printed.holds("vkCreateInstance"));
  CHECK(printed.holds("are framed wrong"));
  /* The batch's own line counted only the commands that framed. */
  CHECK(std::strstr(printed.line(0u), " 1 commands") != nullptr); }

TEST(context_api, remembers_one_context_and_answers_for_no_other)
{ ContextApi api;
  CHECK(!api.known());
  CHECK(!api.names(0u));
  CHECK_EQ(api.api(), ContextApi::unknown_api);
  CHECK(api.invariants_hold());
  api.learn(7u, 1u);
  CHECK(api.known());
  CHECK(api.names(7u));
  CHECK(!api.names(8u));
  CHECK_EQ(api.api(), 1u);
  CHECK(api.invariants_hold());
  /* A context set up again on the same slot replaces what was there. */
  api.learn(7u, 3u);
  CHECK_EQ(api.api(), 3u);
  CHECK(api.invariants_hold()); }
