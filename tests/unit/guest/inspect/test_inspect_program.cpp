#include "core/trace/scope_name.hpp"
#include "core/trace/trace_table.hpp"
#include "guest/inspect/inspect_program.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/suite/adapter_scenario.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_inspect_host.hpp"
#include "harness/guest/mock_kmt.hpp"
#include "harness/guest/scripted_kernel_trace.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/trace/mock_trace_host.hpp"

#include <cstdint>
#include <cstring>

using av::InspectProgram;
using av::MockInspectHost;
using av::MockKmt;
using av::Status;
using Exit = InspectProgram::Exit;
using Kind = av::TraceEscapeRecord::Kind;

namespace {

constexpr std::size_t two_slots = 96u + 2u * 824u;

const std::uint16_t *wide(const char16_t *text) { return reinterpret_cast<const std::uint16_t *>(text); }

std::uint32_t u32_at(const std::uint8_t *bytes, std::size_t offset)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}

std::uint64_t u64_at(const std::uint8_t *bytes, std::size_t offset)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}

/* A two-slot table a process wrote, with one thread inside a function, in a
 * section larger than the table. */
struct ProcessTable {
    av::MockTraceHost trace_host;
    alignas(8) std::uint8_t bytes[two_slots + 100u] = {};
    av::TraceTable table{trace_host};

    ProcessTable()
    {
        CHECK_STATUS(table.initialize(bytes, two_slots, "rvkvk", 42u), Status::ok);
        table.enter(av::ScopeName("IcdRoot::destroy_instance"));
    }
};

struct Rig {
    av::MockPlatform         platform;
    MockKmt                  kmt;
    MockInspectHost          host;
    av::ScriptedKernelTrace  driver;
    InspectProgram           program{host, kmt, platform};

    Rig()
    {
        host.set_clock(777u);
        kmt.add_adapter(av::PciIdentity(av::AdapterScenario::virtio_vendor, av::AdapterScenario::virtio_gpu_device,
                                        av::AdapterScenario::virtio_vendor, av::AdapterScenario::rvk_subsystem),
                        1u);
        kmt.answer_adapter_escapes(&driver);
        driver.set_clock(555u);
    }
    ~Rig()
    {
        CHECK_EQ(platform.live_allocations(), 0u);
        CHECK_EQ(kmt.open_objects(), 0u);
    }
    std::uint32_t kind() const { return u32_at(host.written(), 16u); }
    std::uint32_t length() const { return u32_at(host.written(), 20u); }
    std::uint64_t clock() const { return u64_at(host.written(), 8u); }
    const std::uint8_t *payload() const { return host.written() + 24u; }
    /* An error file's text, terminated. */
    const char *reason()
    {
        CHECK_EQ(kind(), 3u);
        std::memset(text, 0, sizeof text);
        std::memcpy(text, payload(), length() < sizeof text - 1u ? length() : sizeof text - 1u);
        return text;
    }
    char text[256] = {};
};

}  // namespace

TEST(inspect_program_run, anything_but_a_known_command_ending_in_out_is_usage)
{ Rig rig;
  const char16_t *const refused[] = {
      u"rvkinspect", u"rvkinspect list", u"rvkinspect list --out", u"rvkinspect list --in f",
      u"rvkinspect list x --out f", u"rvkinspect trace 42 --out f", u"rvkinspect trace x rvkvk --out f",
      u"rvkinspect trace 42 RVKVK --out f", u"rvkinspect trace 42 rvk-vk --out f",
      u"rvkinspect state 42 \"\" --out f", u"rvkinspect trace 42 sixteenletters16 --out f",
      u"rvkinspect watch 42 rvkvk --out f", u"rvkinspect kernel-trace 1 --out f", u"rvkinspect \u00e9 --out f",
      u"rvkinspect trace 42 rvkvk extra --out f", u"rvkinspect watch --out f", u"rvkinspect trace 42 rvk~vk --out f",
  };
  for (const char16_t *line : refused) CHECK(rig.program.run(wide(line)) == Exit::usage);
  CHECK(rig.program.run(nullptr) == Exit::usage);
  CHECK_EQ(rig.host.writes(), 0u);
  CHECK(std::strcmp(rig.program.usage(), "usage: rvkinspect list|kernel-trace|kernel-state --out FILE\n"
                                         "       rvkinspect trace|state PID MODULE --out FILE\n") == 0); }

TEST(inspect_program_list, names_each_table_found_with_its_scope_and_executable)
{ Rig rig;
  const std::uint8_t header[96] = {};
  rig.host.add_process(10u, "C:\\Games\\game.exe");
  rig.host.add_process(20u, nullptr);
  rig.host.add_process(30u, "C:\\Windows\\notepad.exe");
  rig.host.add_section("Global\\rvk-trace-rvkvk-10", header, sizeof header);
  rig.host.add_section("Local\\rvk-trace-rvkvk-10", header, sizeof header);
  rig.host.add_section("Local\\rvk-trace-rvkgl-20", header, sizeof header);
  rig.host.add_section("Global\\rvk-trace-rvktest-20", header, sizeof header);
  CHECK(rig.program.run(wide(u"rvkinspect list --out C:\\rvk\\list.txt")) == Exit::written);
  const char *const expected = "table pid=10 module=rvkvk scope=global exe=C:\\Games\\game.exe\n"
                               "table pid=20 module=rvkgl scope=local exe=\n"
                               "table pid=20 module=rvktest scope=global exe=\n";
  CHECK_EQ(rig.host.written_size(), std::strlen(expected));
  CHECK(std::memcmp(rig.host.written(), expected, std::strlen(expected)) == 0);
  CHECK(std::strcmp(rig.host.written_path(), "C:\\rvk\\list.txt") == 0);
  /* A global table is found first and the local name of it is never asked. */
  CHECK(std::strncmp(rig.host.journal(),
                     "Global\\rvk-trace-rvkvk-10\nGlobal\\rvk-trace-rvkgl-10\nLocal\\rvk-trace-rvkgl-10\n", 76u) == 0); }

TEST(inspect_program_list, an_empty_machine_is_an_empty_file_and_a_claimed_count_is_bounded)
{ Rig rig;
  CHECK(rig.program.run(wide(u"rvkinspect list --out f")) == Exit::written);
  CHECK_EQ(rig.host.written_size(), 0u);
  rig.host.add_process(10u, "x");
  rig.host.claim_processes(5000u);
  CHECK(rig.program.run(wide(u"rvkinspect list --out f")) == Exit::written); }

TEST(inspect_program_list, without_memory_or_a_file_nothing_is_written)
{ Rig rig;
  for (std::uint32_t nth = 1u; nth <= 2u; nth++) {
    rig.platform.fail_allocation_at(nth);
    CHECK(rig.program.run(wide(u"rvkinspect list --out f")) == Exit::unwritten);
    CHECK_EQ(rig.platform.live_allocations(), 0u);
  }
  CHECK_EQ(rig.host.writes(), 0u);
  rig.host.fail_next_write(Status::no_space);
  CHECK(rig.program.run(wide(u"rvkinspect list --out f")) == Exit::unwritten); }

TEST(inspect_program_trace, copies_a_valid_table_trimmed_to_its_layout)
{ Rig rig;
  ProcessTable process;
  rig.host.add_section("Global\\rvk-trace-rvkvk-42", process.bytes, sizeof process.bytes);
  CHECK(rig.program.run(wide(u"rvkinspect trace 42 rvkvk --out C:\\rvk\\t.bin")) == Exit::written);
  CHECK(std::memcmp(rig.host.written(), "RVKSNAP1", 8u) == 0);
  CHECK_EQ(rig.clock(), 777u);
  CHECK_EQ(rig.kind(), 1u);
  CHECK_EQ(rig.length(), two_slots);
  CHECK_EQ(rig.host.written_size(), 24u + two_slots);
  CHECK(std::memcmp(rig.payload(), process.bytes, two_slots) == 0); }

TEST(inspect_program_trace, falls_back_to_the_session_s_table)
{ Rig rig;
  ProcessTable process;
  rig.host.add_section("Local\\rvk-trace-rvkgl-42", process.bytes, two_slots);
  CHECK(rig.program.run(wide(u"rvkinspect trace 42 rvkgl --out f")) == Exit::written);
  CHECK_EQ(rig.kind(), 1u);
  CHECK(std::strcmp(rig.host.journal(), "Global\\rvk-trace-rvkgl-42\nLocal\\rvk-trace-rvkgl-42\n") == 0); }

TEST(inspect_program_trace, a_missing_or_malformed_table_is_an_error_file)
{ Rig rig;
  CHECK(rig.program.run(wide(u"rvkinspect trace 42 rvkvk --out f")) == Exit::written);
  CHECK(std::strcmp(rig.reason(), "no trace table for rvkvk in process 42") == 0);
  CHECK_EQ(rig.clock(), 777u);
  ProcessTable process;
  process.bytes[0] ^= 1u;
  rig.host.add_section("Global\\rvk-trace-rvkvk-42", process.bytes, two_slots);
  CHECK(rig.program.run(wide(u"rvkinspect trace 42 rvkvk --out f")) == Exit::written);
  CHECK(std::strcmp(rig.reason(), "the table from Global\\rvk-trace-rvkvk-42 is malformed") == 0);
  rig.host.fail_next_write(Status::no_space);
  CHECK(rig.program.run(wide(u"rvkinspect trace 42 rvkvk --out f")) == Exit::unwritten);
  rig.platform.fail_allocation_at(1u);
  CHECK(rig.program.run(wide(u"rvkinspect trace 42 rvkvk --out f")) == Exit::unwritten); }

TEST(inspect_program_state, asks_the_service_and_writes_its_dump)
{ Rig rig;
  std::uint8_t section[64] = {};
  const char   dump[] = "root/0 busy=false";
  section[0] = static_cast<std::uint8_t>(sizeof dump - 1u);
  std::memcpy(section + 4, dump, sizeof dump - 1u);
  rig.host.add_service("Global\\rvk-dump-rvkvk-42", Status::ok);
  rig.host.add_section("Global\\rvk-state-rvkvk-42", section, sizeof section);
  CHECK(rig.program.run(wide(u"rvkinspect state 42 rvkvk --out f")) == Exit::written);
  CHECK_EQ(rig.kind(), 2u);
  CHECK_EQ(rig.length(), sizeof dump - 1u);
  CHECK(std::memcmp(rig.payload(), dump, sizeof dump - 1u) == 0);
  CHECK_EQ(rig.host.waited(), 5000u);
  CHECK(std::strcmp(rig.host.journal(),
                    "Global\\rvk-dump-rvkvk-42\nGlobal\\rvk-done-rvkvk-42\nGlobal\\rvk-state-rvkvk-42\n") == 0); }

TEST(inspect_program_state, a_session_s_service_is_asked_when_there_is_no_global_one)
{ Rig rig;
  const std::uint8_t section[4] = {0, 0, 0, 0};
  rig.host.add_service("Local\\rvk-dump-rvktest-7", Status::ok);
  rig.host.add_section("Local\\rvk-state-rvktest-7", section, sizeof section);
  CHECK(rig.program.run(wide(u"rvkinspect state 7 rvktest --out f")) == Exit::written);
  CHECK_EQ(rig.kind(), 2u);
  CHECK_EQ(rig.length(), 0u); }

TEST(inspect_program_state, every_way_a_service_fails_to_answer_is_an_error_file)
{ Rig rig;
  CHECK(rig.program.run(wide(u"rvkinspect state 42 rvkvk --out f")) == Exit::written);
  CHECK(std::strcmp(rig.reason(), "no state service in that process") == 0);
  Rig slow;
  slow.host.add_service("Global\\rvk-dump-rvkvk-42", Status::invalid_state);
  CHECK(slow.program.run(wide(u"rvkinspect state 42 rvkvk --out f")) == Exit::written);
  CHECK(std::strcmp(slow.reason(), "the state service did not answer in 5 s") == 0);
  Rig lost;
  lost.host.add_service("Global\\rvk-dump-rvkvk-42", Status::ok);
  CHECK(lost.program.run(wide(u"rvkinspect state 42 rvkvk --out f")) == Exit::written);
  CHECK(std::strcmp(lost.reason(), "the state service answered but its dump cannot be opened") == 0);
  /* A length past the section, a section with no room for the length, and a
   * length exactly filling the section. */
  const std::uint8_t long_length[8] = {5, 0, 0, 0, 'a', 'b', 'c', 'd'};
  const std::uint8_t cut[3] = {0, 0, 0};
  const std::uint8_t full[8] = {4, 0, 0, 0, 'a', 'b', 'c', 'd'};
  Rig overlong;
  overlong.host.add_service("Global\\rvk-dump-rvkvk-42", Status::ok);
  overlong.host.add_section("Global\\rvk-state-rvkvk-42", long_length, sizeof long_length);
  CHECK(overlong.program.run(wide(u"rvkinspect state 42 rvkvk --out f")) == Exit::written);
  CHECK(std::strcmp(overlong.reason(), "the state dump is longer than its section") == 0);
  Rig short_section;
  short_section.host.add_service("Global\\rvk-dump-rvkvk-42", Status::ok);
  short_section.host.add_section("Global\\rvk-state-rvkvk-42", cut, sizeof cut);
  CHECK(short_section.program.run(wide(u"rvkinspect state 42 rvkvk --out f")) == Exit::written);
  CHECK(std::strcmp(short_section.reason(), "the state dump is longer than its section") == 0);
  Rig exact;
  exact.host.add_service("Global\\rvk-dump-rvkvk-42", Status::ok);
  exact.host.add_section("Global\\rvk-state-rvkvk-42", full, sizeof full);
  CHECK(exact.program.run(wide(u"rvkinspect state 42 rvkvk --out f")) == Exit::written);
  CHECK_EQ(exact.kind(), 2u);
  CHECK(std::memcmp(exact.payload(), "abcd", 4u) == 0);
  exact.platform.fail_allocation_at(1u);
  CHECK(exact.program.run(wide(u"rvkinspect state 42 rvkvk --out f")) == Exit::unwritten); }

TEST(inspect_program_kernel, writes_the_driver_s_table_and_state_at_the_driver_s_clock)
{ Rig rig;
  ProcessTable driver_table;
  const char   state[] = "adapter/0 devices=1";
  rig.driver.serve(Kind::table, driver_table.bytes, two_slots);
  rig.driver.serve(Kind::state, static_cast<const std::uint8_t *>(static_cast<const void *>(state)),
                   sizeof state - 1u);
  CHECK(rig.program.run(wide(u"rvkinspect kernel-trace --out f")) == Exit::written);
  CHECK_EQ(rig.kind(), 1u);
  CHECK_EQ(rig.clock(), 555u);
  CHECK(std::memcmp(rig.payload(), driver_table.bytes, two_slots) == 0);
  CHECK(rig.program.run(wide(u"rvkinspect kernel-state --out f")) == Exit::written);
  CHECK_EQ(rig.kind(), 2u);
  CHECK_EQ(rig.length(), sizeof state - 1u);
  CHECK(std::memcmp(rig.payload(), state, sizeof state - 1u) == 0); }

TEST(inspect_program_kernel, a_driver_that_cannot_answer_is_an_error_file)
{ Rig rig;
  ProcessTable driver_table;
  driver_table.bytes[8] = 9u;
  rig.driver.serve(Kind::table, driver_table.bytes, two_slots);
  CHECK(rig.program.run(wide(u"rvkinspect kernel-trace --out f")) == Exit::written);
  CHECK(std::strcmp(rig.reason(), "the table from the driver is malformed") == 0);
  CHECK_EQ(rig.clock(), 555u);
  rig.driver.fault_at(1u, av::ScriptedKernelTrace::Fault::other_offset);
  CHECK(rig.program.run(wide(u"rvkinspect kernel-state --out f")) == Exit::written);
  CHECK(std::strcmp(rig.reason(), "the driver's escape failed, status 9") == 0);
  CHECK_EQ(rig.clock(), 777u);
  av::MockPlatform platform;
  MockKmt          bare;
  MockInspectHost  host;
  InspectProgram   lonely(host, bare, platform);
  CHECK(lonely.run(wide(u"rvkinspect kernel-state --out f")) == Exit::written);
  CHECK_EQ(u32_at(host.written(), 16u), 3u);
  CHECK(std::memcmp(host.written() + 24u, "no rvkgpu adapter", 17u) == 0);
  CHECK_EQ(u32_at(host.written(), 20u), 17u); }
