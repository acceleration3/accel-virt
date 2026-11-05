#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::StateWriter;

namespace {

std::size_t unescape(const char *escaped, char *into, std::size_t capacity)
{
    std::size_t length = 0u;
    for (const char *at = escaped; *at != '\0' && length < capacity; at++) {
        if (*at != '\\') {
            into[length++] = *at;
            continue;
        }
        at++;
        if (*at == 'n') {
            into[length++] = '\n';
        } else if (*at == 'r') {
            into[length++] = '\r';
        } else if (*at == 'x') {
            const auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'A' + 10; };
            into[length++] = static_cast<char>(digit(at[1]) * 16 + digit(at[2]));
            at += 2;
        } else {
            into[length++] = *at;
        }
    }
    return length;
}

const char *value_of(const StateWriter &writer)
{
    const char *const equals = std::strchr(writer.dump(), '=');
    return equals != nullptr ? equals + 1 : nullptr;
}

}

TEST(state_writer_text, a_plain_value_is_written_as_it_is)
{ char        bytes[128];
  StateWriter writer(bytes, sizeof bytes);
  writer.open("process", 4u);
  writer.text("name", "avtest.exe");
  writer.close();
  CHECK(std::strcmp(writer.dump(), "process/4 name=avtest.exe") == 0); }

TEST(state_writer_text, a_newline_and_a_carriage_return_are_written_as_two_characters)
{ char        bytes[128];
  StateWriter writer(bytes, sizeof bytes);
  writer.open("p", 0u);
  writer.text("v", "a\nb\rc");
  writer.close();
  CHECK(std::strcmp(writer.dump(), "p/0 v=a\\nb\\rc") == 0);
  CHECK(std::strchr(writer.dump(), '\n') == nullptr);
  CHECK(std::strchr(writer.dump(), '\r') == nullptr); }

TEST(state_writer_text, an_equals_sign_and_a_backslash_are_prefixed_with_a_backslash)
{ char        bytes[128];
  StateWriter writer(bytes, sizeof bytes);
  writer.open("p", 0u);
  writer.text("v", "a=b\\c");
  writer.close();
  CHECK(std::strcmp(writer.dump(), "p/0 v=a\\=b\\\\c") == 0); }

TEST(state_writer_text, a_space_is_written_as_a_hex_escape_so_the_value_stays_one_token)
{ char        bytes[128];
  StateWriter writer(bytes, sizeof bytes);
  writer.open("p", 0u);
  writer.text("v", "my app");
  writer.flag("lost", false);
  writer.close();
  CHECK(std::strcmp(writer.dump(), "p/0 v=my\\x20app lost=false") == 0); }

TEST(state_writer_text, control_characters_and_delete_are_written_as_hex_escapes)
{ char        bytes[128];
  StateWriter writer(bytes, sizeof bytes);
  writer.open("p", 0u);
  writer.text("v", "\x01\x1f\x7f\t");
  writer.close();
  CHECK(std::strcmp(writer.dump(), "p/0 v=\\x01\\x1F\\x7F\\x09") == 0); }

TEST(state_writer_text, the_printable_boundaries_are_exact)
{ char        bytes[128];
  StateWriter writer(bytes, sizeof bytes);
  writer.open("p", 0u);
  writer.text("v", "\x20!~");
  writer.close();
  CHECK(std::strcmp(writer.dump(), "p/0 v=\\x20!~") == 0); }

TEST(state_writer_text, bytes_of_eighty_hex_and_above_pass_through_unchanged)
{ char        bytes[128];
  StateWriter writer(bytes, sizeof bytes);
  writer.open("p", 0u);
  writer.text("v", "\x80\xC3\xA9\xFF");
  writer.close();
  CHECK(std::strcmp(writer.dump(), "p/0 v=\x80\xC3\xA9\xFF") == 0);
  CHECK(writer.invariants_hold()); }

TEST(state_writer_text, no_text_value_is_an_empty_value)
{ char        bytes[128];
  StateWriter writer(bytes, sizeof bytes);
  writer.open("p", 0u);
  writer.text("v", nullptr);
  writer.text("w", "");
  writer.close();
  CHECK(std::strcmp(writer.dump(), "p/0 v= w=") == 0); }

TEST(state_writer_text, every_byte_value_survives_the_escape_and_the_unescape)
{ for (unsigned value = 1u; value < 256u; value++) {
      char        bytes[128];
      StateWriter writer(bytes, sizeof bytes);
      const char  original[4] = {'a', static_cast<char>(value), 'z', '\0'};
      writer.open("p", 0u);
      writer.text("v", original);
      writer.close();
      CHECK(writer.invariants_hold());
      CHECK(!writer.truncated());
      CHECK(std::strchr(writer.dump(), '\n') == nullptr);
      CHECK(std::strchr(writer.dump(), '\r') == nullptr);
      const char *const escaped = value_of(writer);
      CHECK(escaped != nullptr);
      if (escaped == nullptr) continue;
      CHECK(std::strchr(escaped, ' ') == nullptr);
      CHECK(std::strchr(escaped, '=') == nullptr || value == '=');
      char restored[8] = {};
      CHECK_EQ(unescape(escaped, restored, sizeof restored), 3u);
      CHECK_EQ(static_cast<unsigned char>(restored[1]), value);
  } }

TEST(state_writer_text, a_value_cut_by_the_buffer_keeps_the_invariants_at_every_capacity)
{ for (std::size_t capacity = 1u; capacity < 48u; capacity++) {
      char        bytes[64];
      StateWriter writer(bytes, capacity);
      writer.open("process", 1u);
      writer.text("name", "a b\n=\\\x01");
      writer.close();
      CHECK(writer.invariants_hold());
      CHECK(writer.length() < capacity);
      CHECK(std::strlen(writer.dump()) == writer.length());
  } }

TEST(state_writer_text, a_full_buffer_says_it_was_cut)
{ char        bytes[16];
  StateWriter writer(bytes, sizeof bytes);
  writer.open("process", 1u);
  writer.text("name", "\x01\x02\x03\x04\x05\x06\x07\x08");
  writer.close();
  CHECK(writer.truncated());
  CHECK(writer.invariants_hold()); }
