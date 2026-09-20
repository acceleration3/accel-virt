#include "harness/framework/hostile_sweep.hpp"

#include "harness/framework/test_registry.hpp"

#include <cstddef>
#include <cstdint>

namespace {

const std::uint8_t valid_input[4] = {0x01u, 0x02u, 0x03u, 0x04u};

bool rejects_nothing(const std::uint8_t *, std::size_t) { return false; }

bool rejects_everything(const std::uint8_t *, std::size_t) { return true; }

bool rejects_all_but_the_valid_input(const std::uint8_t *bytes,
                                     std::size_t length)
{
    if (length != sizeof valid_input) { return true; }
    for (std::size_t index = 0u; index < length; index++) {
        if (bytes[index] != valid_input[index]) { return true; }
    }
    return false;
}

std::uint32_t expected_cases()
{
    return static_cast<std::uint32_t>(sizeof valid_input + 1u +
                                      8u * sizeof valid_input + 2u);
}

}  // namespace

TEST(hostile_sweep_run, counts_every_case_it_feeds)
{
    const av::HostileSweep sweep(rejects_everything, valid_input,
                                 sizeof valid_input);
    sweep.run();
    CHECK_EQ(sweep.cases_run(), expected_cases());
}

TEST(hostile_sweep_rejections, counts_none_when_the_parser_accepts_everything)
{
    const av::HostileSweep sweep(rejects_nothing, valid_input,
                                 sizeof valid_input);
    sweep.run();
    CHECK_EQ(sweep.rejections(), 0u);
    CHECK_EQ(sweep.cases_run(), expected_cases());
}

TEST(hostile_sweep_rejections, counts_every_case_when_the_parser_refuses_all)
{
    const av::HostileSweep sweep(rejects_everything, valid_input,
                                 sizeof valid_input);
    sweep.run();
    CHECK_EQ(sweep.rejections(), sweep.cases_run());
}

/* The valid input is fed exactly once - as the last truncation - so a parser
 * that accepts only it leaves one accepted case. Every bit flip changes a
 * byte, and neither all-zero nor all-ones equals it. */
TEST(hostile_sweep_rejections, leaves_one_accepted_case_for_the_valid_input)
{
    const av::HostileSweep sweep(rejects_all_but_the_valid_input, valid_input,
                                 sizeof valid_input);
    sweep.run();
    CHECK_EQ(sweep.rejections(), sweep.cases_run() - 1u);
}

TEST(hostile_sweep_rejections, starts_at_zero_before_the_sweep_runs)
{
    const av::HostileSweep sweep(rejects_everything, valid_input,
                                 sizeof valid_input);
    CHECK_EQ(sweep.cases_run(), 0u);
    CHECK_EQ(sweep.rejections(), 0u);
}
