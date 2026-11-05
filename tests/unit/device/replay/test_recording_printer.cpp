#include "core/support/status.hpp"
#include "device/replay/recording_filter.hpp"
#include "device/recording/record_header.hpp"
#include "device/recording/record_kind.hpp"
#include "device/recording/record_kinds.hpp"
#include "device/recording/recording_header.hpp"
#include "device/recording/recording_limit.hpp"
#include "device/replay/recording_printer.hpp"
#include "device/replay/recording_reader.hpp"
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

constexpr const char *path = "print.av";
constexpr std::size_t widest = 8192u;
constexpr std::size_t most_lines = 96u;

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
    std::uint8_t  bytes_[8192];
    std::uint32_t length_;
};

class Printed {
public:
    Printed(const std::uint8_t *bytes, std::size_t length, const RecordingFilter &filter,
            std::uint32_t payload_bytes)
        : memory_(), reader_(bytes, length), printer_(reader_, filter, payload_bytes, memory_), count_(0u), lines_{}
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
    bool        holds(const char *fragment) const
    {
        for (std::size_t index = 0u; index < count_; index++)
            if (std::strstr(lines_[index], fragment) != nullptr) return true;
        return false;
    }

private:
    MockHost         memory_;
    RecordingReader  reader_;
    RecordingPrinter printer_;
    std::size_t      count_;
    char             lines_[most_lines][RecordingPrinter::line_bytes];
};

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
        import[0] = 10u;
        import[16] = 0x00u; import[17] = 0x10u;
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

class Handmade {
public:
    Handmade() : length_(0u), bytes_{}
    {
        const av::RecordingHeader header(ProtocolVersion(1u, 1u), av::RecordKinds::every_kind, 0u, 0u, 0u,
                                         0u, nullptr);
        CHECK_STATUS(header.encode(bytes_, sizeof bytes_), Status::ok);
        length_ = RecordingHeader::encoded_size;
    }

    Handmade &add(av::RecordKind kind, std::uint32_t context, std::uint64_t fence,
                  const std::uint8_t *payload, std::uint32_t payload_length)
    {
        const av::RecordHeader record(kind, context, payload_length, 0u, fence);
        CHECK_STATUS(record.encode(bytes_ + length_, sizeof bytes_ - length_), Status::ok);
        length_ += av::RecordHeader::encoded_size;
        for (std::uint32_t index = 0u; index < payload_length; index++)
            bytes_[length_ + index] = payload[index];
        length_ += payload_length;
        return *this;
    }

    const std::uint8_t *bytes() const { return bytes_; }
    std::size_t         length() const { return length_; }

private:
    std::size_t  length_;
    std::uint8_t bytes_[widest];
};

}

namespace av {

struct RecordingFilterProbe {
    static void invert(RecordingFilter &filter) { filter.first_fence_ = 9u; filter.last_fence_ = 8u; }
};

struct RecordingPrinterProbe {
    static void show_too_much(RecordingPrinter &printer)
    {
        printer.payload_bytes_ = RecordingPrinter::most_payload_bytes + 1u;
    }
    static void lose_the_payload(RecordingPrinter &printer)
    {
        printer.stage_ = RecordingPrinter::Stage::commands;
        printer.payload_ = nullptr;
    }
    static void walk_past_the_batch(RecordingPrinter &printer)
    {
        printer.stage_ = RecordingPrinter::Stage::records;
        printer.command_offset_ = printer.record_.length() + 1u;
    }
};

}

TEST(recording_printer_next, prints_a_line_per_record_and_one_per_command)
{ const Written        written;
  const RecordingFilter filter;
  const Printed        printed(written.bytes(), written.length(), filter, 0u);
  CHECK(!printed.damaged());
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
  char opening[32] = {};
  std::snprintf(opening, sizeof opening, "%08X +", static_cast<unsigned>(RecordingHeader::encoded_size));
  CHECK_EQ(std::strncmp(printed.line(0u), opening, std::strlen(opening)), 0);
  CHECK_EQ(printed.line(2u)[0], ' ');
  CHECK_EQ(printed.line(1u)[0], '0'); }

TEST(recording_printer_next, shows_as_many_payload_bytes_as_it_was_asked_for)
{ const Written written;
  const RecordingFilter filter;
  CHECK(!Printed(written.bytes(), written.length(), filter, 0u).holds("c0c1c2c3"));
  CHECK(!Printed(written.bytes(), written.length(), filter, 0u).holds("C0C1C2C3"));
  CHECK(Printed(written.bytes(), written.length(), filter, 4u).holds("C0C1C2C3"));
  CHECK(!Printed(written.bytes(), written.length(), filter, 4u).holds("C0C1C2C300"));
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
  CHECK(printed.holds("setup api=vulkan"));
  CHECK(printed.holds("blob imported")); }

TEST(recording_printer_next, a_command_filter_keeps_the_batches_that_hold_one)
{ const Written written;
  RecordingFilter filter;
  CHECK_STATUS(filter.keep_command("vkCreateDevice"), Status::ok);
  const Printed printed(written.bytes(), written.length(), filter, 0u);
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
  const Printed printed(cut, written.length() - 4u, filter, 0u);
  CHECK(printed.damaged());
  CHECK_EQ(printed.count(), 10u);
  CHECK(printed.holds("setup api=vulkan"));
  CHECK(printed.holds("fence=2 fault 6 undecodable"));
  CHECK(!printed.holds("blob imported"));
  const char *const last = printed.line(printed.count() - 1u);
  CHECK(std::strstr(last, "a record length passes the end of the recording") != nullptr);
  CHECK(std::strstr(last, "damaged at 00000000") == nullptr);
  CHECK(std::strstr(last, "damaged at ") != nullptr);
  for (std::size_t index = 0u; index + 1u < printed.count(); index++)
      CHECK(std::strstr(printed.line(index), "damaged at ") == nullptr); }

TEST(recording_printer_next, a_file_that_is_no_recording_prints_one_line_of_damage)
{ const std::uint8_t nothing[8] = {};
  const RecordingFilter filter;
  const Printed printed(nothing, sizeof nothing, filter, 0u);
  CHECK_EQ(printed.count(), 1u);
  CHECK(printed.damaged());
  CHECK(std::strstr(printed.line(0u),
                    "damaged at 00000000: the file header is not a recording's") != nullptr); }

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
  CHECK(std::strstr(printed.line(0u), " 1 commands") != nullptr); }

TEST(recording_printer_next, prints_the_time_a_record_was_taken_to_the_nanosecond)
{ MockHost     host;
  WireRecorder recorder(host);
  const std::uint8_t uuid[GpuIdentity::uuid_bytes] = {0x31u};
  host.advance_clock_per_read(1234u);
  CHECK_STATUS(recorder.open(path, 0u, "all", AtCeiling::stop,
                             GpuIdentity(1u, 2u, GpuIdentity::discrete_gpu, uuid, 0u, 0u, "mock"),
                             ProtocolVersion(1u, 1u)),
               Status::ok);
  for (std::uint64_t fence = 1u; fence <= 3u; fence++) recorder.record_fence(1u, fence);
  recorder.close();
  const RecordingFilter filter;
  const Printed         printed(host.recording_bytes(path), host.recording_length(path), filter, 0u);
  CHECK_EQ(printed.count(), 3u);
  CHECK(std::strstr(printed.line(0u), "+1.234 ") != nullptr);
  CHECK(std::strstr(printed.line(1u), "+2.468 ") != nullptr);
  CHECK(std::strstr(printed.line(2u), "+3.702 ") != nullptr); }

TEST(recording_printer_next, pads_the_nanoseconds_of_a_record_taken_inside_a_microsecond)
{ struct Case { std::uint64_t step; const char *shown; };
  const Case cases[] = {{5u, "+0.005 "}, {45u, "+0.045 "}, {450u, "+0.450 "}, {1000u, "+1.000 "}};
  for (const Case &c : cases) {
      MockHost     host;
      WireRecorder recorder(host);
      const std::uint8_t uuid[GpuIdentity::uuid_bytes] = {0x31u};
      host.advance_clock_per_read(c.step);
      CHECK_STATUS(recorder.open(path, 0u, "all", AtCeiling::stop,
                                 GpuIdentity(1u, 2u, GpuIdentity::discrete_gpu, uuid, 0u, 0u, "mock"),
                                 ProtocolVersion(1u, 1u)),
                   Status::ok);
      recorder.record_fence(1u, 1u);
      recorder.close();
      const RecordingFilter filter;
      const Printed         printed(host.recording_bytes(path), host.recording_length(path), filter, 0u);
      CHECK_EQ(printed.count(), 1u);
      CHECK(std::strstr(printed.line(0u), c.shown) != nullptr);
  } }

TEST(recording_printer_next, a_record_whose_payload_is_not_what_its_kind_says_prints_what_it_can)
{ Handmade           made;
  const std::uint8_t nothing[1] = {};
  const std::uint8_t short_fault[2] = {};
  std::uint8_t       far_fault[8] = {};
  far_fault[0] = 99u;
  std::uint8_t       odd_blob[32] = {};
  odd_blob[0] = 9u;
  const std::uint8_t short_blob[8] = {};
  const std::uint8_t short_bytes[4] = {};
  std::uint8_t       whole_bytes[20] = {};
  whole_bytes[0] = 5u;
  made.add(av::RecordKind::context_setup, 1u, 0u, nothing, 0u)
      .add(av::RecordKind::fault, 1u, 1u, short_fault, sizeof short_fault)
      .add(av::RecordKind::fault, 1u, 2u, far_fault, sizeof far_fault)
      .add(av::RecordKind::blob, 1u, 0u, odd_blob, sizeof odd_blob)
      .add(av::RecordKind::blob, 1u, 0u, short_blob, sizeof short_blob)
      .add(av::RecordKind::blob_bytes, 1u, 0u, short_bytes, sizeof short_bytes)
      .add(av::RecordKind::blob_bytes, 1u, 0u, whole_bytes, sizeof whole_bytes);
  const RecordingFilter filter;
  const Printed         printed(made.bytes(), made.length(), filter, 0u);
  CHECK(!printed.damaged());
  CHECK_EQ(printed.count(), 7u);
  CHECK(std::strstr(printed.line(0u), "setup, unreadable") != nullptr);
  CHECK(std::strstr(printed.line(1u), "fault 0") != nullptr);
  CHECK(std::strstr(printed.line(1u), "none") == nullptr);
  CHECK(std::strstr(printed.line(2u), "fault 99") != nullptr);
  CHECK(std::strstr(printed.line(2u), "undecodable") == nullptr);
  CHECK(std::strstr(printed.line(3u), "blob unknown") != nullptr);
  CHECK(std::strstr(printed.line(4u), "blob created") != nullptr);
  CHECK(std::strstr(printed.line(4u), "id=") == nullptr);
  CHECK(std::strstr(printed.line(5u), "blob bytes") != nullptr);
  CHECK(std::strstr(printed.line(5u), "id=") == nullptr);
  CHECK(std::strstr(printed.line(6u), "blob bytes id=5 offset=0 4 bytes") != nullptr); }

TEST(recording_printer_next, prints_every_transport_command_it_can_read_and_says_nothing_it_cannot)
{ Batch batch;
  const std::uint8_t body[32] = {};
  std::uint8_t       release[8] = {};
  release[0] = 0x20u;
  batch.add(CommandHeader::nop_id, nullptr, 0u)
       .add(CommandHeader::import_blob_id, body, 4u)
       .add(CommandHeader::release_blob_id, release, sizeof release)
       .add(CommandHeader::release_blob_id, body, 4u);
  Handmade made;
  made.add(av::RecordKind::batch, 1u, 1u, batch.bytes(), batch.length());
  const RecordingFilter filter;
  const Printed         printed(made.bytes(), made.length(), filter, 0u);
  CHECK_EQ(printed.count(), 5u);
  CHECK(std::strstr(printed.line(1u), "nop 0 bytes") != nullptr);
  CHECK(std::strstr(printed.line(2u), "import_blob 4 bytes") != nullptr);
  CHECK(std::strstr(printed.line(2u), "resource=") == nullptr);
  CHECK(std::strstr(printed.line(3u), "release_blob 8 bytes memory=0x0000000000000020") != nullptr);
  CHECK(std::strstr(printed.line(4u), "release_blob 4 bytes") != nullptr);
  CHECK(std::strstr(printed.line(4u), "memory=") == nullptr); }

TEST(recording_printer_next, an_empty_batch_is_a_batch_of_no_commands)
{ Handmade              made;
  const std::uint8_t    nothing[1] = {};
  made.add(av::RecordKind::batch, 1u, 1u, nothing, 0u);
  const RecordingFilter filter;
  const Printed         shown(made.bytes(), made.length(), filter, 0u);
  CHECK_EQ(shown.count(), 1u);
  CHECK(std::strstr(shown.line(0u), "batch 0 bytes, 0 commands") != nullptr);
  RecordingFilter named;
  CHECK_STATUS(named.keep_command("vkCreate"), Status::ok);
  CHECK_EQ(Printed(made.bytes(), made.length(), named, 0u).count(), 0u);
  RecordingFilter api;
  CHECK_STATUS(api.keep_api("vulkan"), Status::ok);
  CHECK_EQ(Printed(made.bytes(), made.length(), api, 0u).count(), 0u); }

TEST(recording_printer_next, learns_each_contexts_sub_api_and_forgets_none_of_the_contexts_it_holds)
{ Handmade made;
  std::uint8_t setup[ContextSetup::encoded_size] = {};
  CHECK_STATUS(ContextSetup(ProtocolVersion(1u, 0u), SubApi::vulkan, 40u, 16u, 41u, 1u, 42u)
                   .encode(setup, sizeof setup),
               Status::ok);
  std::uint8_t opencl[ContextSetup::encoded_size] = {};
  CHECK_STATUS(ContextSetup(ProtocolVersion(1u, 0u), SubApi::opencl, 40u, 16u, 41u, 1u, 42u)
                   .encode(opencl, sizeof opencl),
               Status::ok);
  std::uint8_t opengl[ContextSetup::encoded_size] = {};
  CHECK_STATUS(ContextSetup(ProtocolVersion(1u, 0u), SubApi::opengl, 40u, 16u, 41u, 1u, 42u)
                   .encode(opengl, sizeof opengl),
               Status::ok);
  std::uint8_t d3d[ContextSetup::encoded_size] = {};
  CHECK_STATUS(ContextSetup(ProtocolVersion(1u, 0u), SubApi::d3d, 40u, 16u, 41u, 1u, 42u).encode(d3d, sizeof d3d),
               Status::ok);
  const std::uint8_t nothing[1] = {};
  made.add(av::RecordKind::context_setup, 1u, 0u, setup, sizeof setup)
      .add(av::RecordKind::context_setup, 2u, 0u, opencl, sizeof opencl)
      .add(av::RecordKind::context_setup, 3u, 0u, opengl, sizeof opengl)
      .add(av::RecordKind::context_setup, 4u, 0u, d3d, sizeof d3d)
      .add(av::RecordKind::context_setup, 1u, 0u, setup, sizeof setup)
      .add(av::RecordKind::fence_completed, 1u, 7u, nothing, 0u)
      .add(av::RecordKind::fence_completed, 2u, 8u, nothing, 0u)
      .add(av::RecordKind::fence_completed, 3u, 10u, nothing, 0u)
      .add(av::RecordKind::fence_completed, 4u, 11u, nothing, 0u)
      .add(av::RecordKind::fence_completed, 9u, 9u, nothing, 0u);
  struct Case { const char *api; const char *kept_fence; const char *other_fence; };
  const Case cases[] = {{"vulkan", "ctx=1 fence=7", "ctx=3 fence=10"},
                        {"opencl", "ctx=2 fence=8", "ctx=1 fence=7"},
                        {"opengl", "ctx=3 fence=10", "ctx=2 fence=8"},
                        {"d3d", "ctx=4 fence=11", "ctx=3 fence=10"}};
  for (const Case &c : cases) {
      RecordingFilter only;
      CHECK_STATUS(only.keep_api(c.api), Status::ok);
      const Printed kept(made.bytes(), made.length(), only, 0u);
      char names[32] = {};
      std::snprintf(names, sizeof names, "setup api=%s", c.api);
      CHECK(kept.holds(names));
      CHECK(kept.holds(c.kept_fence));
      CHECK(!kept.holds(c.other_fence));
      CHECK(!kept.holds("ctx=9 fence=9"));
  } }

TEST(recording_printer_next, a_recording_of_many_contexts_remembers_each_ones_api)
{ Handmade made;
  std::uint8_t vulkan_setup[ContextSetup::encoded_size] = {};
  std::uint8_t opengl_setup[ContextSetup::encoded_size] = {};
  CHECK_STATUS(ContextSetup(ProtocolVersion(1u, 0u), SubApi::vulkan, 40u, 16u, 41u, 1u, 42u)
                   .encode(vulkan_setup, sizeof vulkan_setup),
               Status::ok);
  CHECK_STATUS(ContextSetup(ProtocolVersion(1u, 0u), SubApi::opengl, 40u, 16u, 41u, 1u, 42u)
                   .encode(opengl_setup, sizeof opengl_setup),
               Status::ok);
  constexpr std::uint32_t contexts = 40u;
  for (std::uint32_t context = 1u; context <= contexts; context++) {
      const std::uint8_t *const setup = context % 2u == 0u ? vulkan_setup : opengl_setup;
      made.add(av::RecordKind::context_setup, context, 0u, setup, ContextSetup::encoded_size);
  }
  const RecordingFilter filter;
  const Printed         printed(made.bytes(), made.length(), filter, 0u);
  CHECK_EQ(printed.count(), contexts);
  CHECK(!printed.damaged());
  RecordingFilter vulkan;
  CHECK_STATUS(vulkan.keep_api("vulkan"), Status::ok);
  const Printed kept(made.bytes(), made.length(), vulkan, 0u);
  CHECK_EQ(kept.count(), contexts / 2u); }

TEST(recording_printer_invariants, catch_a_printer_that_lost_the_batch_it_is_showing)
{ const Written        written;
  const RecordingFilter filter;
  RecordingReader      reader(written.bytes(), written.length());
  MockHost             memory;
  RecordingPrinter     printer(reader, filter, 0u, memory);
  CHECK(printer.invariants_hold());
  av::RecordingPrinterProbe::show_too_much(printer);
  CHECK(!printer.invariants_hold());

  RecordingReader  second(written.bytes(), written.length());
  RecordingPrinter lost(second, filter, 0u, memory);
  av::RecordingPrinterProbe::lose_the_payload(lost);
  CHECK(!lost.invariants_hold());

  RecordingReader  third(written.bytes(), written.length());
  RecordingPrinter walked(third, filter, 0u, memory);
  av::RecordingPrinterProbe::walk_past_the_batch(walked);
  CHECK(!walked.invariants_hold()); }

TEST(recording_printer_invariants, catch_a_filter_that_stopped_making_sense)
{ const Written    written;
  RecordingFilter  filter;
  RecordingReader  reader(written.bytes(), written.length());
  MockHost         memory;
  RecordingPrinter printer(reader, filter, 0u, memory);
  CHECK(printer.invariants_hold());
  av::RecordingFilterProbe::invert(filter);
  CHECK(!printer.invariants_hold()); }
