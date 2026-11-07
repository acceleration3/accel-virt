#include "harness/framework/test_registry.hpp"
#include "proto/context/mem_info_text.hpp"

#include <cstdint>
#include <cstring>

using av::MemInfoText;

namespace {

std::uint64_t total_of(const char *text) { return MemInfoText(text, std::strlen(text)).total_bytes(); }

}

TEST(mem_info_text, the_first_line_of_a_real_meminfo_is_the_total_in_kibibytes)
{ CHECK_EQ(total_of("MemTotal:       31457280 kB\nMemFree:        12345 kB\n"), 31457280ull * 1024u); }

TEST(mem_info_text, the_total_is_found_wherever_the_line_is)
{ CHECK_EQ(total_of("MemFree: 1 kB\nMemAvailable: 2 kB\nMemTotal: 16384 kB\nBuffers: 3 kB\n"), 16384ull * 1024u);
  CHECK_EQ(total_of("MemTotal:\t8192 kB"), 8192ull * 1024u); }

TEST(mem_info_text, text_with_no_total_or_a_broken_one_answers_zero)
{ CHECK_EQ(total_of(""), 0u);
  CHECK_EQ(total_of("MemFree: 100 kB\n"), 0u);
  CHECK_EQ(total_of("MemTotal: kB\n"), 0u);
  CHECK_EQ(total_of("MemTotal: lots kB\n"), 0u);
  CHECK_EQ(total_of("SwapMemTotal: 100 kB\n"), 0u);
  CHECK_EQ(MemInfoText(nullptr, 40u).total_bytes(), 0u); }

TEST(mem_info_text, a_number_too_large_to_hold_in_bytes_answers_zero)
{ CHECK_EQ(total_of("MemTotal: 99999999999999999999 kB\n"), 0u);
  CHECK_EQ(total_of("MemTotal: 18014398509481984 kB\n"), 0u); }

TEST(mem_info_text, a_total_cut_off_by_the_buffer_reads_only_what_is_there)
{ const char text[] = "MemTotal: 123456 kB";
  CHECK_EQ(MemInfoText(text, 11u).total_bytes(), 1024u);
  CHECK_EQ(MemInfoText(text, 14u).total_bytes(), 1234ull * 1024u);
  CHECK_EQ(MemInfoText(text, 8u).total_bytes(), 0u); }

TEST(mem_info_text, the_available_memory_is_found_apart_from_the_total_and_free_lines)
{ const char *const text = "MemTotal: 16384 kB\nMemFree: 1 kB\nMemAvailable:    2048 kB\nBuffers: 3 kB\n";
  CHECK_EQ(MemInfoText(text, std::strlen(text)).available_bytes(), 2048ull * 1024u);
  const char *const none = "MemTotal: 16384 kB\nMemFree: 1 kB\n";
  CHECK_EQ(MemInfoText(none, std::strlen(none)).available_bytes(), 0u); }
