#include "guest/inspect/inspect_host.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/guest/mock_inspect_host.hpp"
#include "harness/guest/scripted_module_resolver.hpp"

#include <cstdint>
#include <cstring>

using av::MockInspectHost;
using av::Status;

TEST(mock_inspect_host, processes_lists_what_fits_and_a_claimed_count_once)
{ MockInspectHost mock;
  av::InspectHost &host = mock;
  mock.add_process(1u, "a.exe");
  mock.add_process(2u, nullptr);
  std::uint32_t pids[1] = {};
  CHECK_EQ(host.processes(pids, 1u), 1u);
  CHECK_EQ(pids[0], 1u);
  mock.claim_processes(9u);
  CHECK_EQ(host.processes(pids, 1u), 9u);
  CHECK_EQ(host.processes(pids, 1u), 1u); }

TEST(mock_inspect_host, executable_answers_a_known_path_cut_to_fit)
{ MockInspectHost mock;
  av::InspectHost &host = mock;
  mock.add_process(1u, "C:\\long\\a.exe");
  mock.add_process(2u, nullptr);
  char path[6] = {};
  CHECK_STATUS(host.executable(1u, path, sizeof path), Status::ok);
  CHECK(std::strcmp(path, "C:\\lo") == 0);
  CHECK_STATUS(host.executable(2u, path, sizeof path), Status::not_found);
  CHECK_STATUS(host.executable(3u, path, sizeof path), Status::not_found); }

TEST(mock_inspect_host, copy_section_copies_at_most_the_capacity_and_journals_the_name)
{ MockInspectHost mock;
  av::InspectHost &host = mock;
  const std::uint8_t bytes[] = {1, 2, 3, 4};
  mock.add_section("Global\\s", bytes, sizeof bytes);
  std::uint8_t copy[3] = {};
  std::size_t  copied = 9u;
  CHECK_STATUS(host.copy_section("Global\\s", copy, sizeof copy, copied), Status::ok);
  CHECK_EQ(copied, 3u);
  CHECK_EQ(copy[2], 3u);
  CHECK_STATUS(host.copy_section("Local\\s", copy, sizeof copy, copied), Status::not_found);
  CHECK(std::strcmp(mock.journal(), "Global\\s\nLocal\\s\n") == 0); }

TEST(mock_inspect_host, request_state_answers_as_scripted_and_write_file_keeps_the_file)
{ MockInspectHost mock;
  av::InspectHost &host = mock;
  mock.add_service("d", Status::invalid_state);
  CHECK_STATUS(host.request_state("d", "e", 12u), Status::invalid_state);
  CHECK_EQ(mock.waited(), 12u);
  CHECK_STATUS(host.request_state("x", "y", 1u), Status::not_found);
  const std::uint8_t head[] = {'h'};
  const std::uint8_t body[] = {'b', 'c'};
  CHECK_STATUS(host.write_file("p", head, 1u, body, 2u), Status::ok);
  CHECK_EQ(mock.written_size(), 3u);
  CHECK(std::memcmp(mock.written(), "hbc", 3u) == 0);
  mock.fail_next_write(Status::no_space);
  CHECK_STATUS(host.write_file("q", nullptr, 0u, body, 2u), Status::no_space);
  CHECK(std::strcmp(mock.written_path(), "p") == 0);
  CHECK_EQ(mock.writes(), 2u);
  mock.set_clock(5u);
  CHECK_EQ(host.clock(), 5u); }

TEST(scripted_module_resolver, resolves_inside_a_module_only)
{ av::ScriptedModuleResolver modules;
  modules.add(0x1000u, 0x100u, "m.dll");
  char          name[16] = {};
  std::uint64_t offset = 0u;
  CHECK(modules.resolve(0x1010u, name, sizeof name, offset));
  CHECK(std::strcmp(name, "m.dll") == 0);
  CHECK_EQ(offset, 0x10u);
  CHECK(!modules.resolve(0x1100u, name, sizeof name, offset));
  CHECK(!modules.resolve(0x0FFFu, name, sizeof name, offset));
  CHECK_EQ(modules.asked(), 3u); }
