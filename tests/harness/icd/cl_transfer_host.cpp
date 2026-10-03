#include "harness/icd/cl_transfer_host.hpp"

#include "proto/generated/opencl/cl_protocol_ids.h"

#include <CL/cl.h>

#include <cstdlib>
#include <cstring>

namespace av {

namespace {

constexpr std::uint32_t nobody = ~std::uint32_t{0};

/* The wait list and the event flag every enqueue ends with; this host makes
 * no events, so a command that asks for one is one it cannot answer. */
bool read_tail(WireReader &payload)
{
    std::uint64_t waits = 0u;
    std::uint8_t  listed = 0u, wanted = 0u;
    return payload.read_varint(waits) && payload.read_pod(listed) && listed == 0u && payload.read_pod(wanted) &&
           wanted == 0u;
}

bool read_triple(WireReader &payload, std::uint64_t *into)
{
    std::uint8_t  present = 0u;
    std::uint64_t count = 0u;
    if (!payload.read_pod(present) || present == 0u || !payload.read_varint(count) || count != 3u) return false;
    return payload.read_varint(into[0]) && payload.read_varint(into[1]) && payload.read_varint(into[2]);
}

}  // namespace

ClTransferHost::ClTransferHost()
    : request_(static_cast<std::uint8_t *>(std::calloc(1u, request_bytes))),
      reply_(static_cast<std::uint8_t *>(std::calloc(1u, reply_bytes))),
      buffer_(static_cast<std::uint8_t *>(std::calloc(1u, buffer_bytes))),
      image_(static_cast<std::uint8_t *>(std::calloc(1u, image_bytes))),
      scratch_(static_cast<std::uint8_t *>(std::calloc(1u, scratch_bytes))),
      seen_(static_cast<Command *>(std::calloc(max_commands, sizeof(Command)))), commands_(0u), answered_(0u),
      refused_at_(nobody), refusal_(CL_SUCCESS), arena_(nullptr, 0u), blobs_()
{
}

ClTransferHost::~ClTransferHost()
{
    std::free(request_);
    std::free(reply_);
    std::free(buffer_);
    std::free(image_);
    std::free(scratch_);
    std::free(seen_);
}

void ClTransferHost::fill(std::uint8_t seed)
{
    for (std::size_t index = 0u; index < buffer_bytes; ++index)
        buffer_[index] = static_cast<std::uint8_t>(seed + index);
    for (std::size_t z = 0u; z < image_depth; ++z)
        for (std::size_t y = 0u; y < image_height; ++y)
            for (std::size_t x = 0u; x < image_width; ++x) {
                const std::uint32_t value = texel_of(x, y, z);
                std::memcpy(image_ + image_offset(x, y, z), &value, element_bytes);
            }
}

std::uint32_t ClTransferHost::texel_of(std::size_t x, std::size_t y, std::size_t z) const
{
    return static_cast<std::uint32_t>(x) | static_cast<std::uint32_t>(y) << 19u | static_cast<std::uint32_t>(z) << 24u |
           0x80000000u;
}

std::uint8_t ClTransferHost::buffer_at(std::size_t offset) const { return buffer_[offset]; }

std::uint32_t ClTransferHost::texel(std::size_t x, std::size_t y, std::size_t z) const
{
    std::uint32_t value = 0u;
    std::memcpy(&value, image_ + image_offset(x, y, z), element_bytes);
    return value;
}

std::size_t ClTransferHost::image_offset(std::size_t x, std::size_t y, std::size_t z) const
{
    return ((z * image_height + y) * image_width + x) * element_bytes;
}

void ClTransferHost::refuse(std::uint32_t index, std::int32_t error)
{
    refused_at_ = index;
    refusal_ = error;
}

std::uint32_t ClTransferHost::commands() const { return commands_; }

const ClTransferHost::Command &ClTransferHost::command(std::uint32_t index) const { return seen_[index]; }

WireWriter ClTransferHost::request_writer() { return WireWriter(request_, request_bytes); }

/* A command sent without waiting for its answer is applied as the host would
 * apply it, and its answer goes nowhere: a refusal of one is not the caller's
 * to see. */
TransportResult ClTransferHost::send_async(std::uint32_t command_id, WireWriter &writer)
{
    return apply(command_id, writer) ? TransportResult::ok : TransportResult::failed;
}

TransportResult ClTransferHost::send_sync(std::uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    if (!apply(command_id, writer)) return TransportResult::failed;
    reply = WireReader(reply_, answered_);
    return TransportResult::ok;
}

bool ClTransferHost::apply(std::uint32_t command_id, WireWriter &writer)
{
    if (writer.status() != Status::ok || commands_ == max_commands) return false;
    WireReader payload(request_, writer.offset());
    WireWriter answer(reply_, reply_bytes);
    Command   &seen = seen_[commands_];
    seen = Command{};
    seen.id = command_id;
    const bool buffer = command_id == clproto::kCmd_clEnqueueReadBuffer || command_id == clproto::kCmd_clEnqueueWriteBuffer;
    const bool rect =
        command_id == clproto::kCmd_clEnqueueReadBufferRect || command_id == clproto::kCmd_clEnqueueWriteBufferRect;
    const bool read = buffer ? buffer_command(command_id, payload, seen, answer)
                      : rect ? rect_command(command_id, payload, seen, answer)
                             : image_command(command_id, payload, seen, answer);
    ++commands_;
    answered_ = answer.offset();
    return read && answer.status() == Status::ok;
}

bool ClTransferHost::buffer_command(std::uint32_t id, WireReader &payload, Command &seen, WireWriter &answer)
{
    std::uint64_t queue = 0u, buffer = 0u, blocking = 0u, count = 0u;
    std::uint8_t  present = 0u;
    if (!payload.read_pod(queue) || !payload.read_pod(buffer) || buffer != buffer_handle) return false;
    if (!payload.read_varint(blocking) || !payload.read_varint(seen.offset) || !payload.read_varint(seen.bytes))
        return false;
    if (!payload.read_pod(present) || present == 0u || !payload.read_varint(count) || count != seen.bytes) return false;
    seen.blocking = blocking != 0u;
    if (seen.offset > buffer_bytes || seen.bytes > buffer_bytes - seen.offset) return false;
    const bool refused = commands_ == refused_at_;
    if (id == clproto::kCmd_clEnqueueWriteBuffer) {
        if (!payload.read(scratch_, seen.bytes) || !read_tail(payload)) return false;
        if (!refused) std::memcpy(buffer_ + seen.offset, scratch_, seen.bytes);
        answer.write_varint(static_cast<std::uint64_t>(static_cast<std::int64_t>(refused ? refusal_ : CL_SUCCESS)));
        return true;
    }
    if (!read_tail(payload)) return false;
    answer.write_varint(static_cast<std::uint64_t>(static_cast<std::int64_t>(refused ? refusal_ : CL_SUCCESS)));
    answer.write_varint(refused ? std::uint64_t{0} : seen.bytes);
    if (!refused) answer.write(buffer_ + seen.offset, seen.bytes);
    return true;
}

bool ClTransferHost::image_command(std::uint32_t id, WireReader &payload, Command &seen, WireWriter &answer)
{
    if (id != clproto::kCmd_clEnqueueReadImage && id != clproto::kCmd_clEnqueueWriteImage) return false;
    std::uint64_t queue = 0u, image = 0u, blocking = 0u, slice_pitch = 0u, count = 0u;
    std::uint8_t  present = 0u;
    if (!payload.read_pod(queue) || !payload.read_pod(image) || image != image_handle) return false;
    if (!payload.read_varint(blocking) || !read_triple(payload, seen.origin) || !read_triple(payload, seen.region))
        return false;
    if (!payload.read_varint(seen.row_pitch) || !payload.read_varint(slice_pitch)) return false;
    if (!payload.read_pod(present) || present == 0u || !payload.read_varint(count)) return false;
    seen.blocking = blocking != 0u;
    seen.bytes = count;
    if (seen.region[2] != 1u || seen.origin[0] + seen.region[0] > image_width ||
        seen.origin[1] + seen.region[1] > image_height || seen.origin[2] >= image_depth)
        return false;
    const std::size_t row = seen.row_pitch != 0u ? seen.row_pitch : seen.region[0] * element_bytes;
    /* What the generated server insists on: the pitched reach of the region. */
    if (count != (seen.region[1] - 1u) * row + seen.region[0] * element_bytes) return false;
    const bool refused = commands_ == refused_at_;
    if (id == clproto::kCmd_clEnqueueWriteImage) {
        if (!payload.read(scratch_, count) || !read_tail(payload)) return false;
        for (std::size_t y = 0u; y < seen.region[1] && !refused; ++y)
            std::memcpy(image_ + image_offset(seen.origin[0], seen.origin[1] + y, seen.origin[2]), scratch_ + y * row,
                        seen.region[0] * element_bytes);
        answer.write_varint(static_cast<std::uint64_t>(static_cast<std::int64_t>(refused ? refusal_ : CL_SUCCESS)));
        return true;
    }
    if (!read_tail(payload)) return false;
    std::memset(scratch_, 0, count);
    for (std::size_t y = 0u; y < seen.region[1]; ++y)
        std::memcpy(scratch_ + y * row, image_ + image_offset(seen.origin[0], seen.origin[1] + y, seen.origin[2]),
                    seen.region[0] * element_bytes);
    answer.write_varint(static_cast<std::uint64_t>(static_cast<std::int64_t>(refused ? refusal_ : CL_SUCCESS)));
    answer.write_varint(refused ? std::uint64_t{0} : count);
    if (!refused) answer.write(scratch_, count);
    return true;
}

std::size_t ClTransferHost::rect_offset(const Command &seen, std::size_t x, std::size_t y, std::size_t z) const
{
    const std::size_t row = seen.buffer_pitch[0] != 0u ? seen.buffer_pitch[0] : seen.region[0];
    const std::size_t slice = seen.buffer_pitch[1] != 0u ? seen.buffer_pitch[1] : row * seen.region[1];
    return (seen.origin[2] + z) * slice + (seen.origin[1] + y) * row + seen.origin[0] + x;
}

/* As the generated server reads a rectangle and as an OpenCL host applies it:
 * the application's bytes at their own pitch, the buffer's at its. */
bool ClTransferHost::rect_command(std::uint32_t id, WireReader &payload, Command &seen, WireWriter &answer)
{
    std::uint64_t queue = 0u, buffer = 0u, blocking = 0u, host_slice = 0u, count = 0u;
    std::uint8_t  present = 0u;
    if (!payload.read_pod(queue) || !payload.read_pod(buffer) || buffer != buffer_handle) return false;
    if (!payload.read_varint(blocking) || !read_triple(payload, seen.origin) ||
        !read_triple(payload, seen.host_origin) || !read_triple(payload, seen.region))
        return false;
    if (!payload.read_varint(seen.buffer_pitch[0]) || !payload.read_varint(seen.buffer_pitch[1]) ||
        !payload.read_varint(seen.row_pitch) || !payload.read_varint(host_slice))
        return false;
    if (!payload.read_pod(present) || present == 0u || !payload.read_varint(count)) return false;
    seen.blocking = blocking != 0u;
    seen.bytes = count;
    const std::size_t host_row = seen.row_pitch != 0u ? seen.row_pitch : seen.region[0];
    /* What the generated server insists on: the pitched reach from the corner. */
    if (seen.host_origin[0] != 0u || seen.host_origin[1] != 0u || seen.host_origin[2] != 0u || seen.region[2] != 1u ||
        count != (seen.region[1] - 1u) * host_row + seen.region[0])
        return false;
    if (rect_offset(seen, seen.region[0], seen.region[1] - 1u, 0u) > buffer_bytes) return false;
    const bool refused = commands_ == refused_at_;
    if (id == clproto::kCmd_clEnqueueWriteBufferRect) {
        if (!payload.read(scratch_, count) || !read_tail(payload)) return false;
        for (std::size_t y = 0u; y < seen.region[1] && !refused; ++y)
            std::memcpy(buffer_ + rect_offset(seen, 0u, y, 0u), scratch_ + y * host_row, seen.region[0]);
        answer.write_varint(static_cast<std::uint64_t>(static_cast<std::int64_t>(refused ? refusal_ : CL_SUCCESS)));
        return true;
    }
    if (!read_tail(payload)) return false;
    std::memset(scratch_, 0, count);
    for (std::size_t y = 0u; y < seen.region[1]; ++y)
        std::memcpy(scratch_ + y * host_row, buffer_ + rect_offset(seen, 0u, y, 0u), seen.region[0]);
    answer.write_varint(static_cast<std::uint64_t>(static_cast<std::int64_t>(refused ? refusal_ : CL_SUCCESS)));
    answer.write_varint(refused ? std::uint64_t{0} : count);
    if (!refused) answer.write(scratch_, count);
    return true;
}

void             ClTransferHost::end_call() {}
WireArena       &ClTransferHost::decode_arena() { return arena_; }
ClientBlobArena &ClTransferHost::blobs() { return blobs_; }

}  // namespace av
