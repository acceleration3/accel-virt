/* What a SPIR-V module says each kernel parameter is: the probe's own module,
 * and modules built here word by word for every kind and every way a module
 * can be malformed. */
#include "executor/opencl/spirv_argument_kinds.hpp"
#include "guest/probe/shaders/vector_add_spv.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>
#include <initializer_list>

using av::SpirvArgumentKinds;
using av::clproto::KernelArgument;

namespace {

/* A module written an instruction at a time, as spirv-as would lay it out. */
class Module {
public:
    Module() : count_(0u) { header(64u); }

    void header(std::uint32_t bound)
    {
        count_ = 0u;
        const std::uint32_t words[5] = {0x07230203u, 0x00010000u, 0u, bound, 0u};
        for (const std::uint32_t word : words) words_[count_++] = word;
    }
    void op(std::uint32_t opcode, std::initializer_list<std::uint32_t> operands)
    {
        words_[count_++] = (static_cast<std::uint32_t>(operands.size() + 1u) << 16u) | opcode;
        for (const std::uint32_t operand : operands) words_[count_++] = operand;
    }
    /* OpEntryPoint Kernel `function` "name". */
    void entry(std::uint32_t function, const char *name)
    {
        const std::size_t   bytes = std::strlen(name) + 1u;
        const std::uint32_t words = static_cast<std::uint32_t>((bytes + 3u) / 4u);
        words_[count_++] = ((3u + words) << 16u) | 15u;
        words_[count_++] = 6u;
        words_[count_++] = function;
        std::uint32_t packed[16] = {};
        std::memcpy(packed, name, bytes);
        for (std::uint32_t index = 0u; index < words; ++index) words_[count_++] = packed[index];
    }
    void raw(std::uint32_t word) { words_[count_++] = word; }

    const std::uint32_t *words() const { return words_; }
    std::size_t          count() const { return count_; }

private:
    std::uint32_t words_[256] = {};
    std::size_t   count_;
};

/* One kernel, "k", whose parameters are of the types the ids 10 to 17 are:
 * a CrossWorkgroup pointer, a UniformConstant pointer, a Workgroup pointer,
 * an image, a sampler, a pipe, a queue and a Function pointer (a structure by
 * value); and 18 a plain int. */
Module every_kind()
{
    Module module;
    module.entry(30u, "k");
    module.op(21u, {9u, 32u, 0u});
    module.op(32u, {10u, 5u, 9u});
    module.op(32u, {11u, 0u, 9u});
    module.op(32u, {12u, 4u, 9u});
    module.op(25u, {13u, 9u, 1u, 0u, 0u, 0u, 0u, 0u, 0u});
    module.op(26u, {14u});
    module.op(38u, {15u, 0u});
    module.op(37u, {16u});
    module.op(32u, {17u, 7u, 9u});
    module.op(54u, {1u, 30u, 0u, 2u});
    for (std::uint32_t type = 10u; type <= 17u; ++type) module.op(55u, {type, 40u + type});
    module.op(55u, {9u, 60u});
    module.op(56u, {});
    return module;
}

}  // namespace

TEST(spirv_argument_kinds, the_probe_s_module_names_buffers_and_a_ulong)
{ SpirvArgumentKinds kinds;
  KernelArgument argument = KernelArgument::unknown;
  for (std::uint32_t index = 0u; index < 3u; ++index) {
      argument = KernelArgument::unknown;
      CHECK(kinds.kind(av::vector_add_spv, av::vector_add_spv_words, "vector_add", index, argument));
      CHECK(argument == KernelArgument::memory);
  }
  CHECK(kinds.kind(av::vector_add_spv, av::vector_add_spv_words, "offset_add", 3u, argument));
  CHECK(argument == KernelArgument::data);
  /* Past its parameters, and a kernel the module does not have. */
  CHECK(!kinds.kind(av::vector_add_spv, av::vector_add_spv_words, "vector_add", 3u, argument));
  CHECK(!kinds.kind(av::vector_add_spv, av::vector_add_spv_words, "vector", 0u, argument));
  CHECK(!kinds.kind(av::vector_add_spv, av::vector_add_spv_words, "vector_add_more", 0u, argument));
  CHECK(!kinds.kind(av::vector_add_spv, av::vector_add_spv_words, nullptr, 0u, argument)); }

TEST(spirv_argument_kinds, every_kind_a_parameter_can_be)
{ const Module module = every_kind();
  SpirvArgumentKinds kinds;
  const KernelArgument expected[] = {KernelArgument::memory, KernelArgument::memory, KernelArgument::local,
                                     KernelArgument::image,  KernelArgument::sampler, KernelArgument::memory,
                                     KernelArgument::unknown, KernelArgument::data, KernelArgument::data};
  for (std::uint32_t index = 0u; index < 9u; ++index) {
      KernelArgument argument = KernelArgument::unknown;
      CHECK(kinds.kind(module.words(), module.count(), "k", index, argument));
      CHECK(argument == expected[index]);
  }
  /* The same object asked of another module forgets the first's types. */
  KernelArgument argument = KernelArgument::unknown;
  CHECK(kinds.kind(av::vector_add_spv, av::vector_add_spv_words, "offset_add", 3u, argument));
  CHECK(argument == KernelArgument::data); }

TEST(spirv_argument_kinds, a_module_that_is_not_well_formed_answers_nothing)
{ SpirvArgumentKinds kinds;
  KernelArgument argument = KernelArgument::data;
  const Module whole = every_kind();
  CHECK(!kinds.kind(nullptr, whole.count(), "k", 0u, argument));
  CHECK(!kinds.kind(whole.words(), 4u, "k", 0u, argument));
  /* Byte-swapped or not SPIR-V at all. */
  Module swapped = every_kind();
  std::uint32_t words[256] = {};
  std::memcpy(words, swapped.words(), swapped.count() * sizeof words[0]);
  words[0] = 0x03022307u;
  CHECK(!kinds.kind(words, swapped.count(), "k", 0u, argument));
  /* A bound of none, and one past what any compiler writes. */
  words[0] = 0x07230203u;
  words[3] = 0u;
  CHECK(!kinds.kind(words, swapped.count(), "k", 0u, argument));
  words[3] = (1u << 24) + 1u;
  CHECK(!kinds.kind(words, swapped.count(), "k", 0u, argument));
  /* An instruction of no words, and one running past the end. */
  Module empty_instruction;
  empty_instruction.raw(0u);
  CHECK(!kinds.kind(empty_instruction.words(), empty_instruction.count(), "k", 0u, argument));
  Module overrun;
  overrun.raw((9u << 16u) | 15u);
  CHECK(!kinds.kind(overrun.words(), overrun.count(), "k", 0u, argument));
  /* A parameter whose type is an id past the bound. */
  Module outside;
  outside.header(20u);
  outside.entry(5u, "k");
  outside.op(54u, {1u, 5u, 0u, 2u});
  outside.op(55u, {25u, 6u});
  CHECK(!kinds.kind(outside.words(), outside.count(), "k", 0u, argument));
  /* A type declared past the bound is not recorded, and its parameter is
   * then taken for bytes - the same answer as a type nothing declared. */
  Module declared_outside;
  declared_outside.header(20u);
  declared_outside.entry(5u, "k");
  declared_outside.op(26u, {25u});
  declared_outside.op(32u, {7u});
  declared_outside.op(54u, {1u, 5u, 0u, 2u});
  declared_outside.op(55u, {7u, 6u});
  CHECK(kinds.kind(declared_outside.words(), declared_outside.count(), "k", 0u, argument));
  CHECK(argument == KernelArgument::data);
  /* An entry point of another model, and a name with no terminator inside
   * its words, are no kernel of that name. */
  Module vertex;
  vertex.op(15u, {0u, 5u, 0x0000006Bu});
  vertex.op(54u, {1u, 5u, 0u, 2u});
  vertex.op(55u, {9u, 6u});
  CHECK(!kinds.kind(vertex.words(), vertex.count(), "k", 0u, argument));
  Module unterminated;
  unterminated.op(15u, {6u, 5u, 0x6B6B6B6Bu});
  unterminated.op(54u, {1u, 5u, 0u, 2u});
  unterminated.op(55u, {9u, 6u});
  CHECK(!kinds.kind(unterminated.words(), unterminated.count(), "kkkk", 0u, argument));
  /* Short instructions of each kind read nothing past themselves. */
  Module short_ones;
  short_ones.entry(5u, "k");
  short_ones.op(15u, {6u});
  short_ones.op(32u, {});
  short_ones.op(26u, {});
  short_ones.op(54u, {1u});
  short_ones.op(54u, {1u, 5u, 0u, 2u});
  short_ones.op(55u, {9u});
  short_ones.op(55u, {9u, 6u});
  CHECK(kinds.kind(short_ones.words(), short_ones.count(), "k", 0u, argument));
  CHECK(argument == KernelArgument::data); }
