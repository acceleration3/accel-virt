/* An application's property list into the ICD's walk (feature 052, T039).
 *
 * This is the one array the OpenCL ICD reads whose length the application
 * chose: pairs of a name and a value ending at a zero name, and nothing but
 * that terminator says where it stops. The bytes are whatever the application
 * wrote, including a list with no terminator in it at all - which is what the
 * bound is for, and what this proves is never read past.
 *
 * An input is the list, whole 8-byte words; the last word of the buffer is
 * where a read that ran away would land, and the canary after it is what says
 * it did not. */
#include "icd/opencl/cl_property_list.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

using av::clicd::ClPropertyList;

/* Room for a list that fills the bound and the terminator after it, and then
 * a page of canary the walk must never reach. */
constexpr std::size_t   words = ClPropertyList::max_pairs * 2u + 1u;
constexpr std::size_t   guard_words = 64u;
constexpr std::uint64_t canary = 0xDEADBEEFCAFEF00Dull;

void require(bool holds)
{
    if (!holds) __builtin_trap();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    static std::uint64_t list[words + guard_words];
    std::size_t          given = size / sizeof(std::uint64_t);
    if (given > words) given = words;
    std::memcpy(list, data, given * sizeof(std::uint64_t));
    for (std::size_t at = given; at < words + guard_words; at++) list[at] = canary;

    ClPropertyList read;
    const bool     ended = read.read(list);
    require(read.invariants_hold());
    if (!ended) {
        /* A list with no terminator inside the bound keeps nothing: a caller
         * that took its pairs would be acting on a list it never saw the end
         * of. */
        require(read.pairs() == 0u);
    } else {
        require(read.pairs() <= ClPropertyList::max_pairs);
        for (std::uint32_t index = 0u; index < read.pairs(); index++) {
            /* Every pair is one the input actually wrote, in order, and no
             * name is the terminator. */
            require(read.name_at(index) == list[index * 2u]);
            require(read.value_at(index) == list[index * 2u + 1u]);
            require(read.name_at(index) != 0u);
            /* And what names() answers for it is the first pair with that
             * name, which is the pair a caller acts on. */
            std::uint64_t value = canary;
            require(read.names(read.name_at(index), value));
            std::uint32_t first = 0u;
            while (read.name_at(first) != read.name_at(index)) first++;
            require(value == read.value_at(first));
            require(read.repeated() || first == index);
        }
        /* It stopped at a terminator, and that terminator is a word of the
         * buffer rather than somewhere past it. */
        require(read.pairs() * 2u < words + guard_words);
        require(list[read.pairs() * 2u] == 0u);
    }
    /* Past everything the input wrote, nothing moved. */
    for (std::size_t at = (given > words ? words : given); at < words + guard_words; at++)
        require(list[at] == canary);
    /* A name of zero is never answered, whatever the list said. */
    std::uint64_t value = canary;
    require(!read.names(0u, value) && value == canary);
    return 0;
}
