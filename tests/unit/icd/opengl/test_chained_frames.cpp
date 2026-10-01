/* One window's frames as host surfaces: a chain opened in the context - each
 * slot opened on the device by the id the chain shares it as, and made a
 * texture over that memory - a frame copied into a slot and finished
 * one-way, and everything given back on close. The
 * device is a transport that answers by command: opens with memory names,
 * creates with GL names, and every send recorded. */
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/scripted_window_chains.hpp"
#include "icd/opengl/chained_frames.hpp"
#include "proto/client_blob_arena.hpp"
#include "proto/client_transport.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/shared_object_answer.hpp"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include "gl_protocol_ids.h"

#include <cstdint>
#include <cstring>

namespace {

using av::ClientTransport;
using av::CommandHeader;
using av::ScriptedWindowChains;
using av::SharedObjectAnswer;
using av::Status;
using av::TransportResult;
using av::WireReader;
using av::WireWriter;
using av::glicd::ChainedFrames;

constexpr std::uint32_t first_memory = 0x700u;
constexpr std::uint32_t first_name = 40u;
constexpr std::uint64_t luid = 0x1234u;
constexpr std::uint32_t width = 64u;
constexpr std::uint32_t height = 32u;

/* The device and host GL behind one context. */
class ScriptedGlHost final : public ClientTransport {
public:
    static constexpr std::uint32_t max_sends = 64u;
    static constexpr std::size_t   slot_bytes = 4096u;

    ScriptedGlHost() : request_{}, reply_{}, commands_{}, async_{}, payloads_{}, sends_(0u), opens_(0u),
                       names_(first_name), refuse_open_at_(0u), arena_(nullptr, 0u), blobs_() {}

    /* The nth open from now (1-based) is refused; 0 refuses none. */
    void refuse_open_at(std::uint32_t nth) { refuse_open_at_ = nth; }

    std::uint32_t sends() const { return sends_; }
    std::uint32_t command(std::uint32_t index) const { return commands_[index]; }
    bool          async(std::uint32_t index) const { return async_[index]; }
    const std::uint8_t *payload(std::uint32_t index) const { return payloads_[index]; }
    std::uint32_t count(std::uint32_t id) const
    {
        std::uint32_t found = 0u;
        for (std::uint32_t index = 0u; index < sends_; index++) found += commands_[index] == id ? 1u : 0u;
        return found;
    }
    /* The index of the last send of `id`; max_sends for none. */
    std::uint32_t last(std::uint32_t id) const
    {
        std::uint32_t found = max_sends;
        for (std::uint32_t index = 0u; index < sends_; index++)
            if (commands_[index] == id) found = index;
        return found;
    }

    WireWriter request_writer() override { return WireWriter(request_, slot_bytes); }
    TransportResult send_async(std::uint32_t command_id, WireWriter &writer) override
    {
        return record(command_id, writer, true);
    }
    TransportResult send_sync(std::uint32_t command_id, WireWriter &writer, WireReader &reply) override
    {
        if (record(command_id, writer, false) != TransportResult::ok) return TransportResult::failed;
        WireWriter answer(reply_, slot_bytes);
        if (command_id == CommandHeader::open_shared_object_id) {
            opens_++;
            const bool refused = refuse_open_at_ == opens_;
            const SharedObjectAnswer opened(refused ? SharedObjectAnswer::unknown_object : SharedObjectAnswer::done,
                                            refused ? 0u : first_memory + opens_ - 1u);
            static_cast<void>(opened.encode(reply_, SharedObjectAnswer::encoded_size));
            reply = WireReader(reply_, SharedObjectAnswer::encoded_size);
            return TransportResult::ok;
        }
        if (command_id == CommandHeader::release_shared_object_id) {
            static_cast<void>(SharedObjectAnswer(SharedObjectAnswer::done, 0u).encode(reply_, SharedObjectAnswer::encoded_size));
            reply = WireReader(reply_, SharedObjectAnswer::encoded_size);
            return TransportResult::ok;
        }
        /* glCreate*: target (textures only), count, then the names asked for. */
        WireReader asked(payloads_[sends_ - 1u], slot_bytes);
        std::uint64_t target = 0u, n = 0u;
        if (command_id == av::glproto::kCmd_glCreateTextures) static_cast<void>(asked.read_varint(target));
        static_cast<void>(asked.read_varint(n));
        answer.write_varint(n * 4u);
        for (std::uint64_t made = 0u; made < n; made++) answer.write_pod<std::uint32_t>(names_++);
        reply = WireReader(reply_, answer.offset());
        return TransportResult::ok;
    }
    void                  end_call() override {}
    av::WireArena        &decode_arena() override { return arena_; }
    av::ClientBlobArena  &blobs() override { return blobs_; }

private:
    TransportResult record(std::uint32_t command_id, WireWriter &writer, bool one_way)
    {
        if (writer.status() != Status::ok || sends_ == max_sends) return TransportResult::failed;
        commands_[sends_] = command_id;
        async_[sends_] = one_way;
        std::memcpy(payloads_[sends_], request_, writer.offset());
        sends_++;
        return TransportResult::ok;
    }

    std::uint8_t        request_[slot_bytes];
    std::uint8_t        reply_[slot_bytes];
    std::uint32_t       commands_[max_sends];
    bool                async_[max_sends];
    std::uint8_t        payloads_[max_sends][slot_bytes];
    std::uint32_t       sends_;
    std::uint32_t       opens_;
    std::uint32_t       names_;
    std::uint32_t       refuse_open_at_;
    av::WireArena       arena_;
    av::ClientBlobArena blobs_;
};

std::uint64_t u64_at(const std::uint8_t *bytes, std::size_t offset)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, bytes + offset, sizeof value);
    return value;
}

std::uint64_t varint(WireReader &reader)
{
    std::uint64_t value = 0u;
    CHECK(reader.read_varint(value));
    return value;
}

}  // namespace

/* Each slot is opened by the id the chain shares it as, at its size, and made
 * a texture of the window's colour format over the memory the device named. */
TEST(chained_frames, a_chain_opens_every_slot_over_the_memory_the_device_names)
{ static ScriptedGlHost host;
  ScriptedWindowChains chains;
  chains.refuse_opens(false);
  ChainedFrames frames;
  int window = 0;
  CHECK(frames.open(host, chains, &window, luid, width, height, true));
  CHECK(frames.opened());
  /* As many slots as an icd::WindowChains chain offers. */
  CHECK_EQ(ChainedFrames::slot_count, av::icd::WindowChains::max_slots);
  CHECK_EQ(chains.opens(), 1u);
  CHECK(frames.fits(&window, width, height));
  CHECK(!frames.fits(&window, width, height + 1u));
  CHECK(frames.invariants_hold());
  CHECK_EQ(chains.last_luid(), luid);
  CHECK_EQ(chains.last_width(), width);
  CHECK_EQ(host.count(CommandHeader::open_shared_object_id), ChainedFrames::slot_count);
  /* The second open names the second slot, at the bytes the chain shared it as. */
  const std::uint8_t *second = host.payload(1u);
  CHECK_EQ(u64_at(second, 0u), ScriptedWindowChains::first_id + 1u);
  CHECK_EQ(u64_at(second, 24u), std::uint64_t{width} * height * 4u);
  CHECK_EQ(host.count(av::glproto::kCmd_glTextureStorageMem2DEXT), ChainedFrames::slot_count);
  WireReader storage(host.payload(host.last(av::glproto::kCmd_glTextureStorageMem2DEXT)), ScriptedGlHost::slot_bytes);
  CHECK_EQ(varint(storage), first_name + 1u);
  CHECK_EQ(varint(storage), 1u);
  CHECK_EQ(varint(storage), std::uint64_t{ChainedFrames::gl_srgb8_alpha8});
  CHECK_EQ(varint(storage), width);
  CHECK_EQ(varint(storage), height);
  CHECK_EQ(varint(storage), first_memory + 1u);
  CHECK(frames.invariants_hold()); }

/* A frame is copied into a slot whole, as it is stored, and a glFinish follows
 * it one-way. */
TEST(chained_frames, a_frame_is_copied_whole_and_finished_one_way)
{ static ScriptedGlHost host;
  ScriptedWindowChains chains;
  chains.refuse_opens(false);
  ChainedFrames frames;
  int window = 0;
  CHECK(frames.open(host, chains, &window, luid, width, height, false));
  const std::uint32_t before = host.sends();
  frames.draw(host, 7u, 1u);
  CHECK_EQ(host.sends(), before + 2u);
  CHECK_EQ(host.command(before), av::glproto::kCmd_glCopyImageSubData);
  WireReader copy(host.payload(before), ScriptedGlHost::slot_bytes);
  std::uint64_t arguments[15] = {};
  for (std::uint64_t &argument : arguments) argument = varint(copy);
  CHECK_EQ(arguments[0], 7u);
  CHECK_EQ(arguments[1], std::uint64_t{ChainedFrames::gl_texture_2d});
  /* Into the second slot's texture, at its corner, the whole frame. */
  CHECK_EQ(arguments[6], first_name + 1u);
  CHECK_EQ(arguments[9], 0u);
  CHECK_EQ(arguments[10], 0u);
  CHECK_EQ(arguments[12], width);
  CHECK_EQ(arguments[13], height);
  CHECK_EQ(arguments[14], 1u);
  CHECK_EQ(host.command(before + 1u), av::glproto::kCmd_glFinish);
  CHECK(host.async(before + 1u));

  /* A slot the chain does not have draws nothing. */
  frames.draw(host, 7u, ChainedFrames::slot_count);
  CHECK_EQ(host.sends(), before + 2u); }

/* A chain that cannot be had, or a slot the device will not open, leaves
 * nothing held: what was opened is released and the chain closed. */
TEST(chained_frames, a_refused_chain_or_slot_holds_nothing)
{ static ScriptedGlHost host;
  ScriptedWindowChains chains;
  ChainedFrames frames;
  int window = 0;
  CHECK(!frames.open(host, chains, &window, luid, width, height, false));
  CHECK_EQ(host.sends(), 0u);
  CHECK(!frames.opened());
  CHECK(!frames.open(host, chains, nullptr, luid, width, height, false));
  CHECK(!frames.open(host, chains, &window, luid, 0u, height, false));

  chains.refuse_opens(false);
  host.refuse_open_at(2u);
  CHECK(!frames.open(host, chains, &window, luid, width, height, false));
  CHECK(!frames.opened());
  CHECK(frames.invariants_hold());
  /* The first slot's memory went back, and the chain was closed. */
  CHECK_EQ(host.count(CommandHeader::release_shared_object_id), 1u);
  CHECK_EQ(u64_at(host.payload(host.last(CommandHeader::release_shared_object_id)), 0u),
           ScriptedWindowChains::first_id);
  CHECK_EQ(chains.closes(), 1u);
  CHECK(chains.last_closed() == &window); }

/* Closing deletes the slots' names and releases their memory; reopening at a
 * new size keeps the window's chain, and another window's is closed. */
TEST(chained_frames, close_gives_everything_back_and_a_new_window_closes_the_old_chain)
{ static ScriptedGlHost host;
  ScriptedWindowChains chains;
  chains.refuse_opens(false);
  ChainedFrames frames;
  int first = 0, second = 0;
  CHECK(frames.open(host, chains, &first, luid, width, height, false));
  CHECK(frames.open(host, chains, &first, luid, width * 2u, height, false));
  CHECK_EQ(chains.closes(), 0u);
  CHECK_EQ(host.count(CommandHeader::release_shared_object_id), ChainedFrames::slot_count);
  CHECK(frames.open(host, chains, &second, luid, width, height, false));
  CHECK_EQ(chains.closes(), 1u);
  CHECK(chains.last_closed() == &first);
  frames.close(&host, chains);
  CHECK(!frames.opened());
  CHECK_EQ(chains.closes(), 2u);
  CHECK(chains.last_closed() == &second);
  CHECK_EQ(host.count(av::glproto::kCmd_glDeleteTextures), 3u);
  CHECK_EQ(host.count(CommandHeader::release_shared_object_id), 3u * ChainedFrames::slot_count);
  /* With the context gone there is nothing to send: only the chain goes. */
  CHECK(frames.open(host, chains, &first, luid, width, height, false));
  const std::uint32_t sent = host.sends();
  frames.close(nullptr, chains);
  CHECK_EQ(host.sends(), sent);
  CHECK_EQ(chains.closes(), 3u); }
