#include "core/support/status.hpp"
#include "guest/inspect/escape_reader.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_kernel_trace.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/escape/trace_escape_record.hpp"

#include <cstdint>
#include <cstring>

using av::AvgpuAdapters;
using av::EscapeReader;
using av::MockKmt;
using av::ScriptedKernelTrace;
using av::Status;
using Kind = av::TraceEscapeRecord::Kind;
using Fault = ScriptedKernelTrace::Fault;

namespace av {

struct EscapeReaderProbe {
    static void count_without_bytes(EscapeReader &reader, std::uint64_t length)
    {
        reader.length_ = static_cast<std::size_t>(length);
    }
};

}  // namespace av

namespace {

constexpr std::uint32_t window = EscapeReader::window_bytes;

struct Rig {
    av::MockPlatform    platform;
    MockKmt             kmt;
    ScriptedKernelTrace driver;
    std::uint8_t        data[3u * window + 5u];

    Rig()
    {
        kmt.add_adapter(av::PciIdentity(0x10DEu, 0x2684u, 0x10DEu, 0x0001u), 1u);
        kmt.add_adapter(av::PciIdentity(av::PciIdentity::virtio_vendor, av::AvgpuAdapters::virtio_gpu_device,
                                        av::PciIdentity::virtio_vendor, av::AvgpuAdapters::av_subsystem),
                        1u);
        kmt.answer_adapter_escapes(&driver);
        for (std::size_t byte = 0u; byte < sizeof data; byte++) data[byte] = static_cast<std::uint8_t>(byte * 7u + 1u);
        driver.serve(Kind::table, data, sizeof data);
        driver.serve(Kind::state, data, 10u);
        driver.set_clock(4242u);
    }
};

}  // namespace

TEST(escape_reader_read, reads_every_window_of_the_driver_s_table_in_order)
{ Rig rig;
  {
    EscapeReader reader(rig.kmt, rig.platform);
    CHECK(reader.invariants_hold());
    CHECK_EQ(reader.length(), 0u);
    CHECK_STATUS(reader.read(Kind::table), Status::ok);
    CHECK(reader.invariants_hold());
    CHECK_EQ(reader.length(), sizeof rig.data);
    CHECK(std::memcmp(reader.bytes(), rig.data, sizeof rig.data) == 0);
    CHECK_EQ(reader.clock(), 4242u);
    CHECK_EQ(rig.driver.answers(), 4u);
    /* The escape names the adapter alone, with a 16 KiB window after the record. */
    CHECK_EQ(rig.kmt.last_escape_size(), MockKmt::max_private);
    std::uint8_t sent[40u + window] = {};
    std::memcpy(sent, rig.kmt.last_escape(), 40u);
    av::TraceEscapeRecord last;
    CHECK_STATUS(last.decode(sent, sizeof sent), Status::ok);
    CHECK_EQ(last.offset(), 3u * window);
    CHECK_EQ(last.window(), window);
    CHECK_STATUS(reader.read(Kind::state), Status::ok);
    CHECK_EQ(reader.length(), 10u);
    CHECK(std::memcmp(reader.bytes(), rig.data, 10u) == 0);
  }
  CHECK_EQ(rig.platform.live_allocations(), 0u);
  CHECK_EQ(rig.kmt.open_objects(), 0u); }

TEST(escape_reader_read, an_empty_dump_is_one_answer_and_no_bytes)
{ Rig rig;
  rig.driver.serve(Kind::state, nullptr, 0u);
  EscapeReader reader(rig.kmt, rig.platform);
  CHECK_STATUS(reader.read(Kind::state), Status::ok);
  CHECK_EQ(reader.length(), 0u);
  CHECK(reader.bytes() == nullptr);
  CHECK_EQ(rig.driver.answers(), 1u);
  CHECK(reader.invariants_hold()); }

TEST(escape_reader_read, a_dump_of_exactly_one_window_takes_one_answer)
{ Rig rig;
  rig.driver.serve(Kind::table, rig.data, window);
  EscapeReader reader(rig.kmt, rig.platform);
  CHECK_STATUS(reader.read(Kind::table), Status::ok);
  CHECK_EQ(reader.length(), window);
  CHECK_EQ(rig.driver.answers(), 1u); }

TEST(escape_reader_read, refuses_a_driver_that_misbehaves_at_any_window_and_holds_nothing)
{ const Fault faults[] = {Fault::short_window, Fault::grown_total, Fault::refused, Fault::other_offset,
                          Fault::zero_window};
  for (const Fault fault : faults) {
    for (std::uint32_t nth = 1u; nth <= 4u; nth++) {
      Rig rig;
      rig.driver.fault_at(nth, fault);
      EscapeReader reader(rig.kmt, rig.platform);
      const Status read = reader.read(Kind::table);
      /* A total grown at the first answer is contradicted by the second. */
      CHECK(read != Status::ok);
      CHECK_EQ(reader.length(), 0u);
      CHECK(reader.bytes() == nullptr);
      CHECK_EQ(reader.clock(), 0u);
      CHECK(reader.invariants_hold());
      CHECK_EQ(rig.platform.live_allocations(), 0u);
      CHECK_EQ(rig.kmt.open_objects(), 0u);
    }
  } }

TEST(escape_reader_read, a_total_past_64_mib_is_too_large_and_one_at_it_is_read)
{ Rig rig;
  rig.driver.claim_total(EscapeReader::max_total + 1u);
  EscapeReader reader(rig.kmt, rig.platform);
  CHECK_STATUS(reader.read(Kind::table), Status::too_large);
  CHECK_EQ(rig.driver.answers(), 1u);
  CHECK_EQ(rig.platform.live_allocations(), 0u);
  rig.driver.claim_total(4u * window);
  CHECK_STATUS(reader.read(Kind::table), Status::ok);
  CHECK_EQ(reader.length(), 4u * window);
  CHECK_EQ(reader.bytes()[3u * window + 5u], 0u); }

TEST(escape_reader_read, the_largest_dump_is_read_whole)
{ Rig rig;
  rig.driver.claim_total(EscapeReader::max_total);
  EscapeReader reader(rig.kmt, rig.platform);
  CHECK_STATUS(reader.read(Kind::table), Status::ok);
  CHECK_EQ(reader.length(), EscapeReader::max_total);
  CHECK(reader.invariants_hold()); }

TEST(escape_reader_read, no_adapter_no_memory_and_a_refused_escape_are_refusals)
{ av::MockPlatform platform;
  MockKmt            bare;
  EscapeReader       lonely(bare, platform);
  CHECK_STATUS(lonely.read(Kind::table), Status::not_found);
  Rig rig;
  EscapeReader reader(rig.kmt, rig.platform);
  rig.platform.fail_allocation_at(1u);
  CHECK_STATUS(reader.read(Kind::table), Status::no_memory);
  CHECK_EQ(reader.length(), 0u);
  rig.kmt.fail_at(MockKmt::Call::escape, 1u, 0xC0000022u);
  CHECK(reader.read(Kind::table) != Status::ok);
  rig.kmt.answer_adapter_escapes(nullptr);
  CHECK_STATUS(reader.read(Kind::state), Status::invalid_parameter);
  CHECK(reader.invariants_hold()); }

TEST(escape_reader_invariants, fail_for_bytes_counted_but_not_held)
{ Rig rig;
  EscapeReader reader(rig.kmt, rig.platform);
  av::EscapeReaderProbe::count_without_bytes(reader, 5u);
  CHECK(!reader.invariants_hold());
  av::EscapeReaderProbe::count_without_bytes(reader, EscapeReader::max_total + 1u);
  CHECK(!reader.invariants_hold());
  av::EscapeReaderProbe::count_without_bytes(reader, 0u); }
