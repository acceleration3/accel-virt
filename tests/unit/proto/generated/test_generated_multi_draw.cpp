#include "harness/framework/test_registry.hpp"
#include "harness/transport/mock_transport.hpp"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"
#include "proto/generated/vulkan/vk_default_dispatch.h"
#include "proto/generated/vulkan/vk_dispatch_client.h"
#include "proto/generated/vulkan/vk_dispatch_server.h"
#include "proto/generated/vulkan/vk_handle_translation.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"

#include <cstdint>
#include <cstring>

using av::MockTransport;
using av::WireArena;
using av::WireReader;
using av::WireWriter;
namespace vkproto = av::vkproto;

namespace {

class CapturingHost final : public vkproto::DefaultServerDispatch {
public:
    void on_vkCmdDrawMultiEXT(VkCommandBuffer, uint32_t count, const VkMultiDrawInfoEXT *info, uint32_t instances,
                              uint32_t first_instance, uint32_t stride) override
    {
        plain_calls++;
        drawn = count;
        plain_stride = stride;
        instance_count = instances;
        first = first_instance;
        for (uint32_t at = 0u; at < count && at < 8u; at++) plain[at] = info[at];
    }
    void on_vkCmdDrawMultiIndexedEXT(VkCommandBuffer, uint32_t count, const VkMultiDrawIndexedInfoEXT *info,
                                     uint32_t, uint32_t, uint32_t stride, const int32_t *offset) override
    {
        indexed_calls++;
        drawn = count;
        indexed_stride = stride;
        has_offset = offset != nullptr;
        if (offset != nullptr) shared_offset = *offset;
        for (uint32_t at = 0u; at < count && at < 8u; at++) indexed[at] = info[at];
    }

    uint32_t                  plain_calls = 0u;
    uint32_t                  indexed_calls = 0u;
    uint32_t                  drawn = 0u;
    uint32_t                  plain_stride = 0u;
    uint32_t                  indexed_stride = 0u;
    uint32_t                  instance_count = 0u;
    uint32_t                  first = 0u;
    bool                      has_offset = false;
    int32_t                   shared_offset = 0;
    VkMultiDrawInfoEXT        plain[8] = {};
    VkMultiDrawIndexedInfoEXT indexed[8] = {};
};

struct Padded {
    VkMultiDrawInfoEXT info;
    uint32_t           padding;
};

struct PaddedIndexed {
    VkMultiDrawIndexedInfoEXT info;
    uint32_t                  padding;
};

bool replay(MockTransport &transport, uint32_t command, CapturingHost &host, size_t cut = ~size_t{0})
{
    uint8_t   block[4096];
    WireArena arena(block, sizeof block);
    uint8_t   answer[16] = {};
    WireWriter reply(answer, sizeof answer);
    const size_t length = cut < transport.last_length() ? cut : transport.last_length();
    WireReader payload(transport.last_payload(), length);
    return vkproto::dispatch_command(host, command, payload, reply, arena);
}

}

TEST(generated_multi_draw, a_strided_array_arrives_at_the_host_packed_with_the_host_s_own_stride)
{ MockTransport transport;
  vkproto::HandleIdentity handles;
  const Padded draws[3] = {{{10u, 3u}, 0xDEADu}, {{20u, 4u}, 0xBEEFu}, {{30u, 5u}, 0xF00Du}};
  vkproto::av_vkCmdDrawMultiEXT(transport, handles, reinterpret_cast<VkCommandBuffer>(0x1234u), 3u, &draws[0].info, 2u,
                                7u, sizeof(Padded));
  CHECK_EQ(transport.last_command(), static_cast<uint32_t>(vkproto::kCmd_vkCmdDrawMultiEXT));
  CapturingHost host;
  CHECK(replay(transport, vkproto::kCmd_vkCmdDrawMultiEXT, host));
  CHECK_EQ(host.plain_calls, 1u);
  CHECK_EQ(host.drawn, 3u);
  CHECK_EQ(host.plain_stride, static_cast<uint32_t>(sizeof(VkMultiDrawInfoEXT)));
  CHECK_EQ(host.instance_count, 2u);
  CHECK_EQ(host.first, 7u);
  CHECK_EQ(host.plain[0].firstVertex, 10u);
  CHECK_EQ(host.plain[1].vertexCount, 4u);
  CHECK_EQ(host.plain[2].firstVertex, 30u);
  CHECK_EQ(host.plain[2].vertexCount, 5u); }

TEST(generated_multi_draw, an_indexed_array_carries_one_vertex_offset_or_none)
{ MockTransport transport;
  vkproto::HandleIdentity handles;
  const PaddedIndexed draws[2] = {{{1u, 6u, -3}, 1u}, {{7u, 9u, 11}, 2u}};
  const int32_t       shared = -42;
  vkproto::av_vkCmdDrawMultiIndexedEXT(transport, handles, reinterpret_cast<VkCommandBuffer>(0x1234u), 2u,
                                       &draws[0].info, 1u, 0u, sizeof(PaddedIndexed), &shared);
  CapturingHost with_offset;
  CHECK(replay(transport, vkproto::kCmd_vkCmdDrawMultiIndexedEXT, with_offset));
  CHECK(with_offset.has_offset);
  CHECK_EQ(with_offset.shared_offset, -42);
  CHECK_EQ(with_offset.indexed_stride, static_cast<uint32_t>(sizeof(VkMultiDrawIndexedInfoEXT)));
  CHECK_EQ(with_offset.indexed[1].firstIndex, 7u);
  CHECK_EQ(with_offset.indexed[1].indexCount, 9u);
  CHECK_EQ(with_offset.indexed[1].vertexOffset, 11);

  vkproto::av_vkCmdDrawMultiIndexedEXT(transport, handles, reinterpret_cast<VkCommandBuffer>(0x1234u), 2u,
                                       &draws[0].info, 1u, 0u, sizeof(PaddedIndexed), nullptr);
  CapturingHost without;
  CHECK(replay(transport, vkproto::kCmd_vkCmdDrawMultiIndexedEXT, without));
  CHECK(!without.has_offset);
  CHECK_EQ(without.indexed[0].vertexOffset, -3); }

TEST(generated_multi_draw, a_count_larger_than_the_frame_holds_is_refused_not_read_past)
{ MockTransport transport;
  vkproto::HandleIdentity handles;
  const VkMultiDrawInfoEXT draws[2] = {{1u, 2u}, {3u, 4u}};
  vkproto::av_vkCmdDrawMultiEXT(transport, handles, reinterpret_cast<VkCommandBuffer>(0x1234u), 2u, draws, 1u, 0u,
                                sizeof(VkMultiDrawInfoEXT));
  const size_t whole = transport.last_length();
  CHECK(whole > 8u);
  for (size_t cut = 0u; cut < whole; cut++) {
      CapturingHost host;
      CHECK(!replay(transport, vkproto::kCmd_vkCmdDrawMultiEXT, host, cut));
      CHECK_EQ(host.plain_calls, 0u);
  }
  uint8_t forged[256] = {};
  std::memcpy(forged, transport.last_payload(), whole);
  const uint32_t huge = 0x7FFFFFFFu;
  std::memcpy(forged + 8u, &huge, sizeof huge);
  uint8_t       block[4096];
  WireArena     arena(block, sizeof block);
  uint8_t       answer[16] = {};
  WireWriter    reply(answer, sizeof answer);
  WireReader    payload(forged, whole);
  CapturingHost host;
  CHECK(!vkproto::dispatch_command(host, vkproto::kCmd_vkCmdDrawMultiEXT, payload, reply, arena));
  CHECK_EQ(host.plain_calls, 0u); }
