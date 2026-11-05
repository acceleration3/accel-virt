#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/d3d/d3d9_state_block.hpp"
#include "icd/d3d/d3d9_state_entry.hpp"
#include "icd/d3d/d3d9_state_shadow.hpp"

#include <cstdint>

using av::MockPlatform;
using av::Status;
using av::d3d::D3d9StateBlock;
using av::d3d::D3d9StateEntry;
using av::d3d::D3d9StateShadow;
using Kind = av::d3d::D3d9StateEntry::Kind;

namespace {

constexpr std::uint32_t rs_cullmode = 22u;
constexpr std::uint32_t rs_zenable = 7u;

D3d9StateEntry render(std::uint32_t state, std::uint32_t value)
{
    return D3d9StateEntry(Kind::render, state, 0u, &value, 1u);
}

D3d9StateEntry constant(std::uint32_t stage, std::uint32_t reg, std::uint32_t first)
{
    const std::uint32_t words[4] = {first, first + 1u, first + 2u, first + 3u};
    return D3d9StateEntry(Kind::constant_f, stage, reg, words, 4u);
}

}

TEST(d3d9_state_entry, names_the_same_state_whatever_its_value)
{
    const D3d9StateEntry cull_ccw = render(rs_cullmode, 3u);
    const D3d9StateEntry cull_none = render(rs_cullmode, 1u);
    CHECK(cull_ccw.names(cull_none));
    CHECK(!cull_ccw.names(render(rs_zenable, 3u)));
    CHECK(!constant(0u, 4u, 1u).names(constant(1u, 4u, 1u)));
    CHECK(!constant(0u, 4u, 1u).names(constant(0u, 5u, 1u)));
    D3d9StateEntry taken = cull_ccw;
    taken.take_value(cull_none);
    CHECK_EQ(taken.value()[0], 1u);
    CHECK(taken.kind() == Kind::render);
    CHECK_EQ(taken.a(), rs_cullmode);
    CHECK_EQ(taken.b(), 0u);
    const std::uint32_t six[6] = {1u, 2u, 3u, 4u, 5u, 6u};
    const D3d9StateEntry clipped(Kind::viewport, 0u, 0u, six, 6u);
    CHECK_EQ(clipped.value()[3], 4u);
    const D3d9StateEntry short_value(Kind::viewport, 0u, 0u, six, 2u);
    CHECK_EQ(short_value.value()[2], 0u);
    CHECK(D3d9StateEntry().kind() == Kind::none);
}

TEST(d3d9_state_entry, keeps_a_handle_whole_beside_two_words)
{
    const std::uintptr_t address = sizeof(void *) == 8u ? static_cast<std::uintptr_t>(0x123456789abcdef0ull)
                                                        : static_cast<std::uintptr_t>(0x9abcdef0u);
    void *const          handle = reinterpret_cast<void *>(address);
    const D3d9StateEntry stream(Kind::stream, 3u, handle, 64u, 32u);
    CHECK(stream.kind() == Kind::stream);
    CHECK_EQ(stream.a(), 3u);
    CHECK_EQ(stream.b(), 0u);
    CHECK(stream.handle() == handle);
    CHECK_EQ(stream.value()[0], 0x9abcdef0u);
    CHECK_EQ(stream.value()[2], 64u);
    CHECK_EQ(stream.value()[3], 32u);
    const D3d9StateEntry none(Kind::texture, 0u, nullptr, 0u, 0u);
    CHECK(none.handle() == nullptr);
    CHECK_EQ(none.value()[1], 0u);
}

TEST(d3d9_state_shadow, keeps_the_last_value_of_each_state_and_nothing_else)
{
    D3d9StateShadow shadow;
    D3d9StateEntry  now;
    CHECK(!shadow.current(render(rs_cullmode, 0u), now));
    CHECK(shadow.set(render(rs_cullmode, 3u)));
    CHECK(shadow.set(render(rs_cullmode, 2u)));
    CHECK(shadow.current(render(rs_cullmode, 0u), now));
    CHECK_EQ(now.value()[0], 2u);
    CHECK(!shadow.current(render(rs_zenable, 0u), now));
    CHECK(shadow.set(constant(0u, 10u, 100u)));
    CHECK(shadow.set(constant(1u, 10u, 200u)));
    CHECK(shadow.current(constant(0u, 10u, 0u), now));
    CHECK_EQ(now.value()[3], 103u);
    CHECK(shadow.current(constant(1u, 10u, 0u), now));
    CHECK_EQ(now.value()[0], 200u);
    const std::uint32_t point = 1u;
    CHECK(shadow.set(D3d9StateEntry(Kind::stage, 257u, 17u, &point, 1u)));
    CHECK(!shadow.current(D3d9StateEntry(Kind::stage, 1u, 17u, nullptr, 0u), now));
    CHECK(shadow.current(D3d9StateEntry(Kind::stage, 257u, 17u, nullptr, 0u), now));
}

TEST(d3d9_state_shadow, refuses_states_out_of_range)
{
    D3d9StateShadow shadow;
    CHECK(!shadow.set(render(256u, 1u)));
    CHECK(!shadow.set(D3d9StateEntry(Kind::stage, 16u + 256u, 1u, nullptr, 0u)));
    CHECK(!shadow.set(D3d9StateEntry(Kind::stage, 0u, 35u, nullptr, 0u)));
    CHECK(!shadow.set(constant(2u, 0u, 0u)));
    CHECK(!shadow.set(constant(0u, 256u, 0u)));
    CHECK(!shadow.set(D3d9StateEntry(Kind::constant_i, 0u, 16u, nullptr, 0u)));
    CHECK(!shadow.set(D3d9StateEntry(Kind::constant_b, 1u, 16u, nullptr, 0u)));
    CHECK(!shadow.set(D3d9StateEntry(Kind::stream, 16u, 0u, nullptr, 0u)));
    CHECK(!shadow.set(D3d9StateEntry(Kind::clip_plane, 6u, 0u, nullptr, 0u)));
    CHECK(!shadow.set(D3d9StateEntry(Kind::texture, 300u, 0u, nullptr, 0u)));
    CHECK(!shadow.set(D3d9StateEntry()));
    const Kind singles[7] = {Kind::vertex_shader, Kind::pixel_shader, Kind::declaration, Kind::indices,
                             Kind::viewport,      Kind::z_range,      Kind::scissor};
    for (const Kind kind : singles) CHECK(shadow.set(D3d9StateEntry(kind, 0u, 0u, nullptr, 0u)));
}

TEST(d3d9_state_block, records_each_state_once_with_its_last_value)
{
    MockPlatform   platform;
    D3d9StateBlock block(platform);
    CHECK_EQ(block.count(), 0u);
    CHECK(block.invariants_hold());
    CHECK_STATUS(block.record(render(rs_cullmode, 3u)), Status::ok);
    CHECK_STATUS(block.record(render(rs_cullmode, 1u)), Status::ok);
    CHECK_EQ(block.count(), 1u);
    CHECK_EQ(block.at(0u).value()[0], 1u);
    for (std::uint32_t reg = 0u; reg < 200u; reg++) CHECK_STATUS(block.record(constant(0u, reg, reg)), Status::ok);
    CHECK_EQ(block.count(), 201u);
    CHECK_EQ(block.at(200u).b(), 199u);
    CHECK_EQ(block.at(1u).value()[0], 0u);
    CHECK(block.invariants_hold());
}

TEST(d3d9_state_block, capture_takes_the_devices_values_now)
{
    MockPlatform    platform;
    D3d9StateBlock  block(platform);
    D3d9StateShadow shadow;
    CHECK_STATUS(block.record(render(rs_cullmode, 3u)), Status::ok);
    CHECK_STATUS(block.record(render(rs_zenable, 1u)), Status::ok);
    CHECK(shadow.set(render(rs_cullmode, 2u)));
    block.capture(shadow);
    CHECK_EQ(block.at(0u).value()[0], 2u);
    CHECK_EQ(block.at(1u).value()[0], 1u);
    CHECK(block.invariants_hold());
}

TEST(d3d9_state_block, a_block_with_no_room_is_left_as_it_was)
{
    MockPlatform   platform;
    D3d9StateBlock block(platform);
    platform.fail_allocation_at(1u);
    CHECK_STATUS(block.record(render(rs_cullmode, 3u)), Status::no_memory);
    CHECK_EQ(block.count(), 0u);
    CHECK(block.invariants_hold());
}

TEST(d3d9_state_block, grows_past_its_first_room_and_keeps_every_entry)
{
    MockPlatform   platform;
    D3d9StateBlock block(platform);
    constexpr std::uint32_t entries = 1000u;
    for (std::uint32_t index = 0u; index < entries; index++) {
        const std::uint32_t value = index + 1u;
        CHECK_STATUS(block.record(D3d9StateEntry(Kind::render, index, index, &value, 1u)), Status::ok);
    }
    CHECK_EQ(block.count(), entries);
    for (std::uint32_t index = 0u; index < entries; index++) CHECK_EQ(block.at(index).value()[0], index + 1u);
    CHECK(block.invariants_hold());
}

TEST(d3d9_state_block, a_block_that_cannot_grow_keeps_what_it_holds)
{
    MockPlatform   platform;
    D3d9StateBlock block(platform);
    constexpr std::uint32_t first_room = 64u;
    for (std::uint32_t index = 0u; index < first_room; index++) {
        const std::uint32_t value = index;
        CHECK_STATUS(block.record(D3d9StateEntry(Kind::render, index, index, &value, 1u)), Status::ok);
    }
    platform.fail_allocation_at(1u);
    const std::uint32_t over = first_room;
    CHECK_STATUS(block.record(D3d9StateEntry(Kind::render, over, over, &over, 1u)), Status::no_memory);
    CHECK_EQ(block.count(), first_room);
    for (std::uint32_t index = 0u; index < first_room; index++) CHECK_EQ(block.at(index).value()[0], index);
    CHECK(block.invariants_hold());
    CHECK_STATUS(block.record(D3d9StateEntry(Kind::render, over, over, &over, 1u)), Status::ok);
    CHECK_EQ(block.count(), first_room + 1u);
    CHECK(block.invariants_hold());
}
