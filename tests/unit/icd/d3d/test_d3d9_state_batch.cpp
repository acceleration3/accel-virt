#include "harness/framework/test_registry.hpp"
#include "icd/d3d/d3d9_state_batch.hpp"

#include <cstdint>

using av::d3d::D3d9StateBatch;

namespace {
constexpr std::uint32_t ddi_colorop = 1u;
constexpr std::uint32_t ddi_addressu = 13u;
constexpr std::uint32_t ddi_minfilter = 17u;
constexpr std::uint32_t ddi_maxanisotropy = 21u;
constexpr std::uint32_t ddi_addressw = 25u;
constexpr std::uint32_t ddi_srgbtexture = 29u;
constexpr std::uint32_t ddi_dmapoffset = 31u;
constexpr std::uint32_t ddi_constant = 32u;
constexpr std::uint32_t ddi_texturemap = 0u;
constexpr std::uint32_t ddi_colorkey = 34u;
constexpr std::uint32_t legacy_address = 12u;
constexpr std::uint32_t samp_addressu = 1u;
constexpr std::uint32_t samp_addressv = 2u;
constexpr std::uint32_t samp_addressw = 3u;
constexpr std::uint32_t samp_minfilter = 6u;
constexpr std::uint32_t samp_maxanisotropy = 10u;
constexpr std::uint32_t samp_srgbtexture = 11u;
constexpr std::uint32_t samp_dmapoffset = 13u;
constexpr std::uint32_t tss_colorop = 1u;
constexpr std::uint32_t tss_constant = 32u;
constexpr std::uint32_t rs_zenable = 7u;
constexpr std::uint32_t rs_cullmode = 22u;
constexpr std::uint32_t rs_scenecapture = 62u;
constexpr std::uint32_t vertex_sampler_0 = 257u;

}

TEST(d3d9_state_batch, render_states_wait_until_taken_and_go_once)
{
    D3d9StateBatch batch;
    CHECK(!batch.pending());
    batch.render_state(rs_zenable, 1u);
    batch.render_state(rs_cullmode, 3u);
    CHECK(batch.pending());
    CHECK_EQ(batch.render_count(), 4u);
    CHECK_EQ(batch.render_values()[0], rs_zenable);
    CHECK_EQ(batch.render_values()[1], 1u);
    CHECK_EQ(batch.render_values()[2], rs_cullmode);
    CHECK_EQ(batch.render_values()[3], 3u);
    batch.taken();
    CHECK(!batch.pending());
    CHECK_EQ(batch.render_count(), 0u);
    CHECK(batch.invariants_hold());
}

TEST(d3d9_state_batch, a_value_the_host_already_has_is_not_sent_again)
{
    D3d9StateBatch batch;
    batch.render_state(rs_cullmode, 3u);
    batch.taken();
    batch.render_state(rs_cullmode, 3u);
    CHECK(!batch.pending());
    batch.render_state(rs_cullmode, 1u);
    CHECK_EQ(batch.render_count(), 2u);
    batch.taken();
    batch.stage_state(0u, ddi_minfilter, 2u);
    batch.taken();
    batch.stage_state(0u, ddi_minfilter, 2u);
    CHECK(!batch.pending());
    batch.render_state(rs_zenable, 0u);
    batch.render_state(rs_zenable, 1u);
    CHECK_EQ(batch.render_count(), 2u);
    CHECK_EQ(batch.render_values()[1], 1u);
    batch.taken();
    batch.forget();
    batch.render_state(rs_zenable, 1u);
    CHECK(batch.pending());
    CHECK(batch.invariants_hold());
}

TEST(d3d9_state_batch, the_ddis_stage_states_split_into_sampler_and_stage_states)
{
    D3d9StateBatch batch;
    batch.stage_state(1u, ddi_colorop, 4u);
    batch.stage_state(2u, ddi_addressu, 3u);
    batch.stage_state(2u, ddi_minfilter, 2u);
    batch.stage_state(3u, ddi_maxanisotropy, 16u);
    batch.stage_state(3u, ddi_addressw, 1u);
    batch.stage_state(4u, ddi_srgbtexture, 1u);
    batch.stage_state(4u, ddi_dmapoffset, 5u);
    batch.stage_state(5u, ddi_constant, 0xff00ff00u);
    CHECK_EQ(batch.stage_count(), 6u);
    const std::uint32_t *stages = batch.stage_values();
    CHECK_EQ(stages[0], 1u);
    CHECK_EQ(stages[1], tss_colorop);
    CHECK_EQ(stages[2], 4u);
    CHECK_EQ(stages[3], 5u);
    CHECK_EQ(stages[4], tss_constant);
    CHECK_EQ(stages[5], 0xff00ff00u);
    CHECK_EQ(batch.sampler_count(), 18u);
    const std::uint32_t expected[18] = {2u, samp_addressu,      3u,  2u, samp_minfilter,    2u,
                                        3u, samp_maxanisotropy, 16u, 3u, samp_addressw,     1u,
                                        4u, samp_srgbtexture,   1u,  4u, samp_dmapoffset,   5u};
    for (std::uint32_t index = 0u; index < 18u; index++) CHECK_EQ(batch.sampler_values()[index], expected[index]);
    CHECK(batch.invariants_hold());
}

TEST(d3d9_state_batch, the_legacy_address_state_sets_both_axes)
{
    D3d9StateBatch batch;
    batch.stage_state(0u, legacy_address, 3u);
    CHECK_EQ(batch.sampler_count(), 6u);
    CHECK_EQ(batch.sampler_values()[1], samp_addressu);
    CHECK_EQ(batch.sampler_values()[4], samp_addressv);
    CHECK_EQ(batch.sampler_values()[5], 3u);
}

TEST(d3d9_state_batch, vertex_samplers_keep_their_numbers_and_stages_stop_at_seven)
{
    D3d9StateBatch batch;
    batch.stage_state(vertex_sampler_0, ddi_minfilter, 1u);
    CHECK_EQ(batch.sampler_count(), 3u);
    CHECK_EQ(batch.sampler_values()[0], vertex_sampler_0);
    batch.stage_state(8u, ddi_colorop, 2u);
    batch.stage_state(vertex_sampler_0, ddi_colorop, 2u);
    CHECK_EQ(batch.stage_count(), 0u);
    batch.stage_state(16u, ddi_minfilter, 1u);
    batch.stage_state(261u, ddi_minfilter, 1u);
    CHECK_EQ(batch.sampler_count(), 3u);
    CHECK(batch.invariants_hold());
}

TEST(d3d9_state_batch, ddi_only_states_go_nowhere)
{
    D3d9StateBatch batch;
    batch.stage_state(0u, ddi_texturemap, 9u);
    batch.stage_state(0u, ddi_colorkey, 9u);
    batch.stage_state(0u, 35u, 9u);
    batch.stage_state(0u, 0xffffffffu, 9u);
    batch.render_state(rs_scenecapture, 1u);
    batch.render_state(6u, 1u);
    batch.render_state(256u, 1u);
    CHECK(!batch.pending());
    CHECK(batch.invariants_hold());
}
