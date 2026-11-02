#include "harness/guest/scripted_d3d11_deferred.hpp"

#include "harness/framework/test_registry.hpp"

#include <cstring>

namespace av {

namespace {
constexpr std::uint64_t starting_commit = std::uint64_t{512} << 20u;

}

ScriptedD3d11Deferred::ScriptedD3d11Deferred()
    : calls_{}, fail_at_{}, fail_with_{}, recording_{}, lists_{}, previous_{}, geometry_{}, finishes_on_{},
      constant_sizes_{}, constant_count_(0u), geometry_bytes_(0u), readback_bytes_(0u), contexts_(0u),
      commit_finish_(0u), commit_bytes_(0), committed_(starting_commit), window_(nullptr), lose_appends_(false),
      lag_(false), live_(false)
{
}

void ScriptedD3d11Deferred::fail(Call call, std::uint32_t nth, std::int32_t result)
{
    fail_at_[static_cast<unsigned>(call)] = nth;
    fail_with_[static_cast<unsigned>(call)] = result;
}

void ScriptedD3d11Deferred::commit_at_finish(std::uint32_t nth, std::int64_t bytes)
{
    commit_finish_ = nth;
    commit_bytes_ = bytes;
}

void ScriptedD3d11Deferred::lose_appends()
{
    lose_appends_ = true;
}

void ScriptedD3d11Deferred::lag_executes()
{
    lag_ = true;
}

std::uint32_t ScriptedD3d11Deferred::calls(Call call) const
{
    return calls_[static_cast<unsigned>(call)];
}

std::uint32_t ScriptedD3d11Deferred::finishes_on(std::uint32_t context) const
{
    return context < most_contexts ? finishes_on_[context] : 0u;
}

bool ScriptedD3d11Deferred::live() const
{
    return live_;
}

void *ScriptedD3d11Deferred::window() const
{
    return window_;
}

std::uint32_t ScriptedD3d11Deferred::constant_size(std::uint32_t buffer) const
{
    return buffer < most_constants ? constant_sizes_[buffer] : 0u;
}

std::uint32_t ScriptedD3d11Deferred::constant_count() const
{
    return constant_count_;
}

std::uint32_t ScriptedD3d11Deferred::geometry_bytes() const
{
    return geometry_bytes_;
}

std::uint32_t ScriptedD3d11Deferred::readback_bytes() const
{
    return readback_bytes_;
}

std::uint32_t ScriptedD3d11Deferred::contexts() const
{
    return contexts_;
}

std::int32_t ScriptedD3d11Deferred::refusal(Call call)
{
    const unsigned index = static_cast<unsigned>(call);
    calls_[index]++;
    if (fail_at_[index] == 0u || calls_[index] < fail_at_[index]) return 0;
    return fail_with_[index];
}

void ScriptedD3d11Deferred::record(std::uint32_t context, std::uint32_t offset, std::uint32_t bytes,
                                   std::uint8_t value)
{
    Writes &writes = recording_[context];
    for (std::uint32_t byte = offset; byte < offset + bytes && byte < head_bytes; byte++) {
        writes.bytes[byte] = value;
        writes.written[byte] = true;
    }
}

void ScriptedD3d11Deferred::apply(const Writes &list)
{
    for (std::uint32_t byte = 0u; byte < head_bytes; byte++)
        if (list.written[byte]) geometry_[byte] = list.bytes[byte];
}

std::int32_t ScriptedD3d11Deferred::create_device(void *window, std::uint32_t width, std::uint32_t height)
{
    const std::int32_t refused = refusal(Call::create_device);
    if (refused != 0) return refused;
    CHECK(width != 0u && height != 0u);
    window_ = window;
    live_ = true;
    return 0;
}

std::int32_t ScriptedD3d11Deferred::create_buffers(const std::uint32_t *sizes, std::uint32_t count,
                                                   std::uint32_t geometry_bytes, std::uint32_t readback_bytes)
{
    const std::int32_t refused = refusal(Call::create_buffers);
    if (refused != 0) return refused;
    CHECK(live_);
    CHECK(count <= most_constants);
    for (std::uint32_t buffer = 0u; buffer < count && buffer < most_constants; buffer++)
        constant_sizes_[buffer] = sizes[buffer];
    constant_count_ = count;
    geometry_bytes_ = geometry_bytes;
    readback_bytes_ = readback_bytes;
    return 0;
}

std::int32_t ScriptedD3d11Deferred::create_contexts(std::uint32_t count)
{
    const std::int32_t refused = refusal(Call::create_contexts);
    if (refused != 0) return refused;
    CHECK(count <= most_contexts);
    contexts_ = count;
    return 0;
}

std::int32_t ScriptedD3d11Deferred::write_constants(std::uint32_t context, std::uint32_t buffer, std::uint8_t)
{
    const std::int32_t refused = refusal(Call::write_constants);
    if (refused != 0) return refused;
    CHECK(context < contexts_);
    CHECK(buffer < constant_count_);
    return 0;
}

std::int32_t ScriptedD3d11Deferred::discard_geometry(std::uint32_t context, std::uint32_t bytes, std::uint8_t value)
{
    const std::int32_t refused = refusal(Call::discard_geometry);
    if (refused != 0) return refused;
    CHECK(context < contexts_);
    CHECK(bytes <= geometry_bytes_);
    record(context, 0u, bytes, value);
    return 0;
}

std::int32_t ScriptedD3d11Deferred::append_geometry(std::uint32_t context, std::uint32_t offset, std::uint32_t bytes,
                                                    std::uint8_t value)
{
    const std::int32_t refused = refusal(Call::append_geometry);
    if (refused != 0) return refused;
    CHECK(context < contexts_);
    CHECK(offset + bytes <= geometry_bytes_);
    if (!lose_appends_) record(context, offset, bytes, value);
    return 0;
}

std::int32_t ScriptedD3d11Deferred::finish(std::uint32_t context)
{
    const std::int32_t refused = refusal(Call::finish);
    if (refused != 0) return refused;
    CHECK(context < contexts_);
    lists_[context] = recording_[context];
    recording_[context] = Writes{};
    finishes_on_[context]++;
    if (calls(Call::finish) == commit_finish_) committed_ += static_cast<std::uint64_t>(commit_bytes_);
    return 0;
}

std::int32_t ScriptedD3d11Deferred::execute(std::uint32_t context)
{
    const std::int32_t refused = refusal(Call::execute);
    if (refused != 0) return refused;
    CHECK(context < contexts_);
    if (lag_) {
        apply(previous_);
        previous_ = lists_[context];
    } else {
        apply(lists_[context]);
    }
    lists_[context] = Writes{};
    return 0;
}

std::int32_t ScriptedD3d11Deferred::present()
{
    const std::int32_t refused = refusal(Call::present);
    if (refused != 0) return refused;
    CHECK(live_);
    return 0;
}

std::int32_t ScriptedD3d11Deferred::read_geometry(std::uint8_t *bytes, std::uint32_t count)
{
    const std::int32_t refused = refusal(Call::read_geometry);
    if (refused != 0) return refused;
    CHECK(count <= readback_bytes_);
    std::memcpy(bytes, geometry_, count < head_bytes ? count : head_bytes);
    return 0;
}

std::uint64_t ScriptedD3d11Deferred::committed_bytes()
{
    return committed_;
}

void ScriptedD3d11Deferred::release()
{
    calls_[static_cast<unsigned>(Call::release)]++;
    live_ = false;
}

}
