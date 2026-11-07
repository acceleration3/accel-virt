#include "executor/d3d/dxbc_container.hpp"
#include "executor/d3d/dxbc_digest.hpp"
#include "executor/d3d/dxbc_signature_reader.hpp"
#include "harness/framework/test_registry.hpp"

#include "fixtures/d3d12/triangle_vs.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

using av::DxbcContainer;
using av::DxbcDigest;
using av::DxbcSignatureReader;

namespace {

class Xorshift {
public:
    explicit Xorshift(std::uint64_t seed) : state_(seed) {}
    std::uint64_t next()
    {
        state_ ^= state_ << 13u;
        state_ ^= state_ >> 7u;
        state_ ^= state_ << 17u;
        return state_;
    }

private:
    std::uint64_t state_;
};

std::vector<std::uint8_t> bytes_of(const std::uint32_t *words, std::size_t count)
{
    std::vector<std::uint8_t> bytes(count * 4u);
    std::memcpy(bytes.data(), words, bytes.size());
    return bytes;
}

std::vector<std::uint8_t> fixture_container()
{
    const std::uint8_t *const raw = reinterpret_cast<const std::uint8_t *>(triangle_vs::container);
    std::uint32_t total = 0u;
    std::memcpy(&total, raw + 24u, sizeof total);
    return std::vector<std::uint8_t>(raw, raw + total);
}

void read_everything(const std::vector<std::uint8_t> &blob)
{
    std::unique_ptr<std::uint8_t[]> tight(new std::uint8_t[blob.empty() ? 1u : blob.size()]);
    if (!blob.empty()) std::memcpy(tight.get(), blob.data(), blob.size());
    const DxbcSignatureReader reader(tight.get(), blob.size());
    std::uint32_t semantic_index = 0u;
    if (reader.found())
        for (std::uint32_t reg = 0u; reg < 16u; reg++) static_cast<void>(reader.semantic_at(reg, semantic_index));
    DxbcDigest digest;
    std::uint8_t out[16];
    digest.compute(tight.get(), blob.size(), out);
}

}

TEST(dxbc_mutations, a_container_cut_at_every_length_is_read_without_leaving_its_bytes)
{ const std::vector<std::uint8_t> whole = fixture_container();
  CHECK(whole.size() > 64u);
  for (std::size_t length = 0u; length <= whole.size(); length++)
      read_everything(std::vector<std::uint8_t>(whole.begin(), whole.begin() + static_cast<std::ptrdiff_t>(length)));
  CHECK(DxbcSignatureReader(whole.data(), whole.size()).found()); }

TEST(dxbc_mutations, a_container_with_bytes_and_words_flipped_is_read_without_leaving_its_bytes)
{ const std::vector<std::uint8_t> whole = fixture_container();
  Xorshift random(0x9E3779B97F4A7C15ull);
  for (unsigned round = 0u; round < 20000u; round++) {
      std::vector<std::uint8_t> blob = whole;
      const unsigned changes = 1u + static_cast<unsigned>(random.next() % 6u);
      for (unsigned change = 0u; change < changes; change++) {
          const std::size_t at = static_cast<std::size_t>(random.next() % blob.size());
          switch (random.next() % 4u) {
          case 0u: blob[at] ^= static_cast<std::uint8_t>(1u << (random.next() % 8u)); break;
          case 1u: blob[at] = static_cast<std::uint8_t>(random.next()); break;
          case 2u: blob[at] = 0xFFu; break;
          default:
              if (at + 4u <= blob.size()) {
                  const std::uint32_t word = static_cast<std::uint32_t>(random.next());
                  std::memcpy(blob.data() + at, &word, sizeof word);
              }
              break;
          }
      }
      read_everything(blob);
  } }

TEST(dxbc_mutations, shader_code_and_dxil_of_any_size_or_header_build_a_container_or_are_refused)
{ Xorshift random(0xD1B54A32D192ED03ull);
  const std::vector<std::uint8_t> program = bytes_of(triangle_vs::program, sizeof triangle_vs::program / 4u);
  for (unsigned round = 0u; round < 20000u; round++) {
      std::vector<std::uint8_t> code = program;
      const std::size_t keep = static_cast<std::size_t>(random.next() % (code.size() + 1u));
      code.resize(keep);
      if (!code.empty() && random.next() % 2u == 0u) code[static_cast<std::size_t>(random.next() % code.size())] ^= 0x5Au;
      const std::size_t capacity = static_cast<std::size_t>(random.next() % 4096u);
      std::unique_ptr<std::uint8_t[]> buffer(new std::uint8_t[capacity == 0u ? 1u : capacity]);
      DxbcContainer container(buffer.get(), capacity);
      const bool added = random.next() % 2u == 0u ? container.add_dxil(code.data(), code.size())
                                                  : container.add_code(code.data(), code.size());
      const std::size_t made = container.finish();
      CHECK(made <= capacity);
      if (!added) continue;
      read_everything(std::vector<std::uint8_t>(buffer.get(), buffer.get() + made));
  } }
