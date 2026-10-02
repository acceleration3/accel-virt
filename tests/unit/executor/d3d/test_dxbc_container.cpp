/* The DXBC container the executor writes from what the DDI hands a driver,
 * the checksum over it that DXVK checks, and the input signature a pipeline
 * reads back out of an application's own container. The checksum is judged
 * against one dxc wrote (tests/fixtures/d3d12/triangle_vs.hpp), and what the
 * writer writes is read back through the reader and the header's own fields. */
#include "executor/d3d/dxbc_container.hpp"
#include "executor/d3d/dxbc_digest.hpp"
#include "executor/d3d/dxbc_signature_reader.hpp"
#include "harness/framework/test_registry.hpp"

#include "fixtures/d3d/triangle_vs.hpp"
#include "fixtures/d3d12/triangle_vs.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::DxbcContainer;
using av::DxbcDigest;
using av::DxbcSignatureReader;
using SignatureEntry = av::d3dproto::SignatureEntry;

namespace {

std::uint32_t u32(const std::uint8_t *bytes, std::size_t at)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, bytes + at, sizeof value);
    return value;
}

const std::uint8_t *dxc_container() { return reinterpret_cast<const std::uint8_t *>(triangle_vs::container); }
const std::uint8_t *vs_tokens() { return reinterpret_cast<const std::uint8_t *>(triangle_vs::tokens); }
const std::uint8_t *dxil_program() { return reinterpret_cast<const std::uint8_t *>(triangle_vs::program); }

/* The chunk of `fourcc` in a container, or the container's size for none. */
std::size_t chunk_at(const std::uint8_t *container, const char *fourcc)
{
    const std::uint32_t count = u32(container, 28u);
    for (std::uint32_t chunk = 0u; chunk < count; chunk++) {
        const std::uint32_t at = u32(container, 32u + chunk * 4u);
        if (std::memcmp(container + at, fourcc, 4u) == 0) return at;
    }
    return u32(container, 24u);
}

}  // namespace

/* ---- DxbcDigest --------------------------------------------------------- */

TEST(dxbc_digest, is_the_checksum_dxc_wrote_into_its_own_container)
{ std::uint8_t digest[DxbcDigest::digest_bytes] = {};
  DxbcDigest().compute(dxc_container(), triangle_vs::container_bytes, digest);
  CHECK(std::memcmp(digest, dxc_container() + 4u, sizeof digest) == 0); }

TEST(dxbc_digest, a_changed_byte_past_the_checksum_changes_it_and_one_before_does_not)
{ static std::uint8_t copy[triangle_vs::container_bytes];
  std::memcpy(copy, dxc_container(), sizeof copy);
  std::uint8_t digest[DxbcDigest::digest_bytes] = {};
  copy[3] ^= 0x01u;   /* the magic is not hashed */
  DxbcDigest().compute(copy, sizeof copy, digest);
  CHECK(std::memcmp(digest, dxc_container() + 4u, sizeof digest) == 0);
  copy[DxbcDigest::hashed_from] ^= 0x01u;
  DxbcDigest().compute(copy, sizeof copy, digest);
  CHECK(std::memcmp(digest, dxc_container() + 4u, sizeof digest) != 0); }

/* Every length leaves a different amount of the last block, which moves where
 * DXBC's finalisation puts its words: the same digest object gives a stable
 * answer for each, and no two neighbouring lengths agree. */
TEST(dxbc_digest, every_tail_length_is_hashed_and_a_reused_digest_starts_over)
{ DxbcDigest digest;
  std::uint8_t previous[DxbcDigest::digest_bytes] = {};
  for (std::size_t size = DxbcDigest::hashed_from + 1u; size < DxbcDigest::hashed_from + 140u; size++) {
      std::uint8_t first[DxbcDigest::digest_bytes] = {};
      std::uint8_t again[DxbcDigest::digest_bytes] = {};
      digest.compute(dxc_container(), size, first);
      digest.compute(dxc_container(), size, again);
      CHECK(std::memcmp(first, again, sizeof first) == 0);
      CHECK(std::memcmp(first, previous, sizeof first) != 0);
      std::memcpy(previous, first, sizeof first);
  } }

TEST(dxbc_digest, a_container_too_short_for_a_header_gets_zeroes)
{ std::uint8_t digest[DxbcDigest::digest_bytes];
  std::memset(digest, 0xAB, sizeof digest);
  DxbcDigest().compute(dxc_container(), DxbcDigest::hashed_from - 1u, digest);
  for (const std::uint8_t byte : digest) CHECK_EQ(byte, 0u);
  std::memset(digest, 0xAB, sizeof digest);
  DxbcDigest().compute(nullptr, 4096u, digest);
  for (const std::uint8_t byte : digest) CHECK_EQ(byte, 0u);
  /* The header alone is a container with nothing past the checksum: hashed. */
  DxbcDigest().compute(dxc_container(), DxbcDigest::hashed_from, digest);
  std::uint32_t nonzero = 0u;
  for (const std::uint8_t byte : digest) nonzero += byte != 0u ? 1u : 0u;
  CHECK(nonzero > 0u); }

/* ---- DxbcContainer ------------------------------------------------------ */

TEST(dxbc_container, writes_a_container_whose_header_chunks_and_checksum_agree)
{ std::uint8_t buffer[4096] = {};
  DxbcContainer container(buffer, sizeof buffer);
  CHECK(container.add_signature(DxbcContainer::Role::input, triangle_vs::inputs, triangle_vs::inputs_count, false));
  CHECK(container.add_signature(DxbcContainer::Role::output, triangle_vs::outputs, triangle_vs::outputs_count, false));
  CHECK(container.add_code(vs_tokens(), sizeof triangle_vs::tokens));
  const std::size_t size = container.finish();
  CHECK(size > 0u);
  CHECK(std::memcmp(buffer, "DXBC", 4u) == 0);
  CHECK_EQ(u32(buffer, 20u), 1u);
  CHECK_EQ(u32(buffer, 24u), static_cast<std::uint32_t>(size));
  CHECK_EQ(u32(buffer, 28u), 3u);
  std::uint8_t digest[DxbcDigest::digest_bytes] = {};
  DxbcDigest().compute(buffer, size, digest);
  CHECK(std::memcmp(digest, buffer + 4u, sizeof digest) == 0);
  /* The tokens unchanged in SHEX, its length the tokens'; every chunk on four bytes. */
  const std::size_t shex = chunk_at(buffer, "SHEX");
  CHECK(shex < size);
  CHECK_EQ(u32(buffer, shex + 4u), static_cast<std::uint32_t>(sizeof triangle_vs::tokens));
  CHECK(std::memcmp(buffer + shex + 8u, vs_tokens(), sizeof triangle_vs::tokens) == 0);
  CHECK_EQ(chunk_at(buffer, "ISG1") % 4u, 0u);
  CHECK_EQ(chunk_at(buffer, "OSG1") % 4u, 0u); }

/* What the executor names each entry is read back by the reader the pipeline
 * builder uses: AV and the register's first component for a plain register,
 * and an input's mask doubled into the components the shader reads. */
TEST(dxbc_container, an_input_signature_is_read_back_by_register)
{ std::uint8_t buffer[4096] = {};
  DxbcContainer container(buffer, sizeof buffer);
  CHECK(container.add_signature(DxbcContainer::Role::input, triangle_vs::inputs, triangle_vs::inputs_count, false));
  const std::size_t size = container.finish();
  const DxbcSignatureReader reader(buffer, size);
  CHECK(reader.found());
  std::uint32_t index = 99u;
  const char *const first = reader.semantic_at(0u, index);
  CHECK(first != nullptr && std::strcmp(first, "AV") == 0);
  CHECK_EQ(index, 0u);
  const char *const second = reader.semantic_at(1u, index);
  CHECK(second != nullptr && std::strcmp(second, "AV") == 0);
  CHECK_EQ(index, 4u);   /* register 1, component x */
  CHECK(reader.semantic_at(2u, index) == nullptr);
  const std::size_t isg1 = chunk_at(buffer, "ISG1");
  CHECK_EQ(u32(buffer, isg1 + 8u + 8u + 6u * 4u), 0x0303u);   /* entry 0's masks: xy exists, xy read */ }

TEST(dxbc_container_add_layout_signature, names_every_register_the_layout_feeds_as_a_whole_float_register)
{ std::uint8_t buffer[4096] = {};
  DxbcContainer container(buffer, sizeof buffer);
  const std::uint32_t registers[] = {0u, 3u, 7u};
  CHECK(container.add_layout_signature(registers, 3u));
  const std::size_t size = container.finish();
  const DxbcSignatureReader reader(buffer, size);
  std::uint32_t index = 0u;
  for (const std::uint32_t reg : registers) {
      const char *const name = reader.semantic_at(reg, index);
      CHECK(name != nullptr && std::strcmp(name, "AV") == 0);
      CHECK_EQ(index, reg * 4u);
  }
  CHECK(reader.semantic_at(1u, index) == nullptr);
  const std::size_t isg1 = chunk_at(buffer, "ISG1");
  CHECK_EQ(u32(buffer, isg1 + 8u + 8u + 4u * 4u), 3u);        /* FLOAT32 */
  CHECK_EQ(u32(buffer, isg1 + 8u + 8u + 6u * 4u), 0x0F0Fu);   /* xyzw */
  std::uint32_t too_many[33] = {};
  DxbcContainer refused(buffer, sizeof buffer);
  CHECK(!refused.add_layout_signature(too_many, 33u)); }

TEST(dxbc_container_semantic_of, names_system_values_render_targets_depth_and_tessellation_factors)
{ std::uint8_t buffer[64] = {};
  const DxbcContainer container(buffer, sizeof buffer);
  struct Case {
      std::uint32_t sv, reg, mask;
      bool          pixel;
      const char   *name;
      std::uint32_t index;
  };
  const Case cases[] = {
      {0u, 2u, 0x4u, false, "AV", 10u},               /* register 2, component z */
      {0u, 2u, 0x0u, false, "AV", 8u},                /* no mask: the register's first */
      {1u, 0u, 0xFu, false, "SV_Position", 0u},
      {2u, 1u, 0x2u, false, "SV_ClipDistance", 0u},
      {3u, 1u, 0x1u, false, "SV_CullDistance", 0u},
      {10u, 0u, 0x1u, false, "SV_SampleIndex", 0u},
      {0u, 3u, 0xFu, true, "SV_Target", 3u},
      {0u, 0xFFFFFFFFu, 0x1u, true, "SV_Depth", 0xFFFFFFFFu},
      {1u, 0u, 0xFu, true, "SV_Position", 0u},         /* a system value is not a target */
      {11u, 0u, 0x1u, false, "SV_TessFactor", 0u},     /* quad edges 0..3 */
      {14u, 0u, 0x1u, false, "SV_TessFactor", 3u},
      {15u, 0u, 0x1u, false, "SV_InsideTessFactor", 0u},
      {16u, 0u, 0x1u, false, "SV_InsideTessFactor", 1u},
      {17u, 0u, 0x1u, false, "SV_TessFactor", 0u},     /* triangle edges 0..2 */
      {19u, 0u, 0x1u, false, "SV_TessFactor", 2u},
      {20u, 0u, 0x1u, false, "SV_InsideTessFactor", 0u},
      {21u, 0u, 0x1u, false, "SV_TessFactor", 1u},     /* line detail */
      {22u, 0u, 0x1u, false, "SV_TessFactor", 0u},     /* line density */
      {23u, 5u, 0x1u, false, "AV", 20u},              /* past the names: a register */
  };
  for (const Case &c : cases) {
      SignatureEntry entry = {};
      entry.system_value = c.sv;
      entry.register_index = c.reg;
      entry.mask = c.mask;
      std::uint32_t index = 0xABCDu;
      const char *const name = container.semantic_of(entry, c.pixel, index);
      if (std::strcmp(name, c.name) != 0 || index != c.index)
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, c.name);
  } }

/* Two clip distances in one signature are SV_ClipDistance 0 and 1, and a
 * tessellation factor's D3D_NAME is the container's, not the token format's. */
TEST(dxbc_container_add_signature, numbers_distances_and_writes_the_container_s_system_values)
{ std::uint8_t buffer[4096] = {};
  DxbcContainer container(buffer, sizeof buffer);
  SignatureEntry entries[4] = {};
  entries[0] = SignatureEntry{2u, 1u, 0xFu, 3u, 0u, 0u};
  entries[1] = SignatureEntry{2u, 2u, 0xFu, 3u, 0u, 0u};
  entries[2] = SignatureEntry{17u, 3u, 0x1u, 3u, 0u, 0u};
  entries[3] = SignatureEntry{0u, 0u, 0xFu, 3u, 0u, 1u};
  CHECK(container.add_signature(DxbcContainer::Role::patch_constant, entries, 4u, false));
  CHECK(container.finish() > 0u);
  const std::size_t psg1 = chunk_at(buffer, "PSG1");
  const std::size_t table = psg1 + 8u + 8u;
  CHECK_EQ(u32(buffer, table + 2u * 4u), 0u);            /* first distance, index 0 */
  CHECK_EQ(u32(buffer, table + 32u + 2u * 4u), 1u);      /* second, index 1 */
  CHECK_EQ(u32(buffer, table + 32u + 3u * 4u), 2u);      /* D3D_NAME_CLIP_DISTANCE */
  CHECK_EQ(u32(buffer, table + 64u + 3u * 4u), 13u);     /* D3D_NAME_FINAL_TRI_EDGE_TESSFACTOR */
  CHECK_EQ(u32(buffer, table + 96u + 0u * 4u), 1u);      /* its stream */
  CHECK_EQ(u32(buffer, table + 64u + 6u * 4u), 0x1u);    /* not an input: no read mask */ }

TEST(dxbc_container_add_signature, a_pixel_shader_s_outputs_are_targets_and_depth)
{ std::uint8_t buffer[4096] = {};
  DxbcContainer container(buffer, sizeof buffer);
  const SignatureEntry outputs[] = {{0u, 1u, 0xFu, 3u, 0u, 0u}, {0u, 0xFFFFFFFFu, 0x1u, 3u, 0u, 0u}};
  CHECK(container.add_signature(DxbcContainer::Role::output, outputs, 2u, true));
  CHECK(container.finish() > 0u);
  const std::size_t table = chunk_at(buffer, "OSG1") + 8u + 8u;
  CHECK_EQ(u32(buffer, table + 3u * 4u), 64u);          /* D3D_NAME_TARGET */
  CHECK_EQ(u32(buffer, table + 2u * 4u), 1u);           /* SV_Target1 */
  CHECK_EQ(u32(buffer, table + 32u + 3u * 4u), 65u); }  /* D3D_NAME_DEPTH */

TEST(dxbc_container_add_code, refuses_a_stream_whose_length_token_disagrees_with_its_size)
{ std::uint8_t buffer[4096] = {};
  DxbcContainer container(buffer, sizeof buffer);
  CHECK(!container.add_code(nullptr, 8u));
  CHECK(!container.add_code(vs_tokens(), 4u));
  CHECK(!container.add_code(vs_tokens(), sizeof triangle_vs::tokens - 2u));
  CHECK(!container.add_code(vs_tokens(), sizeof triangle_vs::tokens - 4u));
  CHECK(container.add_code(vs_tokens(), sizeof triangle_vs::tokens));
  CHECK_EQ(u32(buffer, 0u), 0u);   /* nothing is a container before finish */
  CHECK(container.finish() > 0u);
  CHECK_EQ(u32(buffer, 28u), 1u); }

TEST(dxbc_container_add_dxil, keeps_a_dxil_program_as_it_is_and_refuses_anything_else)
{ static std::uint8_t buffer[8192];
  DxbcContainer container(buffer, sizeof buffer);
  CHECK(!container.add_dxil(nullptr, 16u));
  CHECK(!container.add_dxil(dxil_program(), 8u));
  CHECK(!container.add_dxil(dxil_program(), triangle_vs::program_bytes - 4u));   /* size disagrees */
  static std::uint8_t not_dxil[triangle_vs::program_bytes];
  std::memcpy(not_dxil, dxil_program(), sizeof not_dxil);
  not_dxil[8] = 'X';
  CHECK(!container.add_dxil(not_dxil, sizeof not_dxil));
  CHECK(container.add_dxil(dxil_program(), triangle_vs::program_bytes));
  const std::size_t size = container.finish();
  CHECK(size > 0u);
  const std::size_t dxil = chunk_at(buffer, "DXIL");
  CHECK(dxil < size);
  CHECK_EQ(u32(buffer, dxil + 4u), static_cast<std::uint32_t>(triangle_vs::program_bytes));
  CHECK(std::memcmp(buffer + dxil + 8u, dxil_program(), triangle_vs::program_bytes) == 0); }

TEST(dxbc_container_finish, a_container_that_did_not_fit_is_nothing)
{ std::uint8_t header_only[40] = {};
  DxbcContainer too_small(header_only, sizeof header_only);
  CHECK_EQ(too_small.finish(), 0u);
  DxbcContainer no_buffer(nullptr, 4096u);
  CHECK(!no_buffer.add_code(vs_tokens(), sizeof triangle_vs::tokens));
  CHECK_EQ(no_buffer.finish(), 0u);
  std::uint8_t small[128] = {};
  DxbcContainer short_of_code(small, sizeof small);
  CHECK(!short_of_code.add_code(vs_tokens(), sizeof triangle_vs::tokens));
  CHECK_EQ(short_of_code.finish(), 0u);
  /* Six chunks is all a container here has room to list. */
  std::uint8_t buffer[4096] = {};
  DxbcContainer full(buffer, sizeof buffer);
  for (std::uint32_t chunk = 0u; chunk < 6u; chunk++) CHECK(full.add_layout_signature(nullptr, 0u));
  CHECK(!full.add_layout_signature(nullptr, 0u));
  CHECK_EQ(full.finish(), 0u); }

/* ---- DxbcSignatureReader ------------------------------------------------ */

TEST(dxbc_signature_reader, reads_the_application_s_own_semantics_out_of_a_dxc_container)
{ const DxbcSignatureReader reader(dxc_container(), triangle_vs::container_bytes);
  CHECK(reader.found());
  std::uint32_t index = 99u;
  const char *const position = reader.semantic_at(0u, index);
  CHECK(position != nullptr && std::strcmp(position, "POSITION") == 0);
  CHECK_EQ(index, 0u);
  const char *const color = reader.semantic_at(1u, index);
  CHECK(color != nullptr && std::strcmp(color, "COLOR") == 0);
  CHECK(reader.semantic_at(2u, index) == nullptr); }

TEST(dxbc_signature_reader, finds_nothing_in_what_is_not_a_container_with_an_input_signature)
{ CHECK(!DxbcSignatureReader(nullptr, 64u).found());
  CHECK(!DxbcSignatureReader(dxc_container(), 31u).found());
  static std::uint8_t copy[triangle_vs::container_bytes];
  std::memcpy(copy, dxc_container(), sizeof copy);
  copy[0] = 'X';
  CHECK(!DxbcSignatureReader(copy, sizeof copy).found());
  std::memcpy(copy, dxc_container(), sizeof copy);
  const std::uint32_t too_many = 65u;
  std::memcpy(copy + 28u, &too_many, 4u);
  CHECK(!DxbcSignatureReader(copy, sizeof copy).found());
  /* A container of code alone. */
  std::uint8_t buffer[4096] = {};
  DxbcContainer code(buffer, sizeof buffer);
  CHECK(code.add_code(vs_tokens(), sizeof triangle_vs::tokens));
  const DxbcSignatureReader reader(buffer, code.finish());
  CHECK(!reader.found());
  std::uint32_t index = 0u;
  CHECK(reader.semantic_at(0u, index) == nullptr); }

/* The bytes came off the wire: a name running off the chunk's end, an entry
 * table past it and a chunk longer than the container are each nothing. */
TEST(dxbc_signature_reader, a_signature_running_past_its_chunk_names_nothing)
{ std::uint8_t buffer[4096] = {};
  DxbcContainer container(buffer, sizeof buffer);
  CHECK(container.add_signature(DxbcContainer::Role::input, triangle_vs::inputs, triangle_vs::inputs_count, false));
  const std::size_t size = container.finish();
  const std::size_t isg1 = chunk_at(buffer, "ISG1");
  std::uint32_t index = 0u;
  static std::uint8_t spoiled[4096];
  std::memcpy(spoiled, buffer, size);
  const std::uint32_t far_name = 0x7FFFu;
  std::memcpy(spoiled + isg1 + 8u + 8u + 4u, &far_name, 4u);
  CHECK(DxbcSignatureReader(spoiled, size).semantic_at(0u, index) == nullptr);
  std::memcpy(spoiled, buffer, size);
  const std::uint32_t many = 1000u;
  std::memcpy(spoiled + isg1 + 8u, &many, 4u);
  CHECK(DxbcSignatureReader(spoiled, size).semantic_at(0u, index) == nullptr);
  std::memcpy(spoiled, buffer, size);
  const std::uint32_t long_chunk = 0x100000u;
  std::memcpy(spoiled + isg1 + 4u, &long_chunk, 4u);
  CHECK(!DxbcSignatureReader(spoiled, size).found());
  /* The unspoiled container still reads, so each refusal above was the spoiling. */
  CHECK(DxbcSignatureReader(buffer, size).semantic_at(0u, index) != nullptr); }
