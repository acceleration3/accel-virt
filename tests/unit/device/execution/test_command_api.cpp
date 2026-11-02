#include "device/execution/command_api.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/generated/d3d/d3d_protocol_ids.h"
#include "proto/generated/d3d12/d3d12_protocol_ids.h"
#include "proto/generated/d3d9/d3d9_protocol_ids.h"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/generated/opengl/gl_protocol_ids.h"
#include "proto/submission/command_header.hpp"

#include <cstdint>
#include <cstring>

using av::CommandApi;
using av::SubApi;

namespace {

bool named(const char *text, const char *expected) { return std::strcmp(text, expected) == 0; }

}

TEST(command_api, each_sub_api_sends_its_own_api)
{ CHECK_EQ(CommandApi(SubApi::vulkan).api(), 1u);
  CHECK_EQ(CommandApi(SubApi::opencl).api(), 2u);
  CHECK_EQ(CommandApi(SubApi::opengl).api(), 3u);
  CHECK_EQ(CommandApi(SubApi::d3d).api(), 4u);
  CHECK_EQ(CommandApi(static_cast<SubApi>(16u)).api(), CommandApi::none);
  CHECK_EQ(CommandApi(static_cast<SubApi>(0u)).api(), CommandApi::none);
  CHECK(!CommandApi(static_cast<SubApi>(16u)).known()); }

TEST(command_api, every_api_has_its_name)
{ CHECK(named(CommandApi(av::CommandHeader::transport_api).name(), "transport"));
  CHECK(named(CommandApi(SubApi::vulkan).name(), "vulkan"));
  CHECK(named(CommandApi(SubApi::opencl).name(), "opencl"));
  CHECK(named(CommandApi(SubApi::opengl).name(), "opengl"));
  CHECK(named(CommandApi(SubApi::d3d).name(), "d3d"));
  CHECK(named(CommandApi(std::uint16_t{9u}).name(), "api"));
  CHECK(CommandApi(std::uint16_t{0u}).known()); }

TEST(command_api, a_name_names_its_api_and_nothing_else_does)
{ CommandApi api(CommandApi::none);
  CHECK(api.take_name("d3d"));
  CHECK_EQ(api.api(), 4u);
  CHECK(api.take_name("transport"));
  CHECK_EQ(api.api(), 0u);
  CHECK(api.take_name("opengl"));
  CHECK_EQ(api.api(), 3u);
  CHECK(!api.take_name("open"));
  CHECK(!api.take_name("opengles"));
  CHECK(!api.take_name("metal"));
  CHECK(!api.take_name(""));
  CHECK(!api.take_name(nullptr));
  CHECK_EQ(api.api(), 3u); }

TEST(command_api, a_direct3d_command_is_named_by_whichever_registry_has_it)
{ const CommandApi d3d(SubApi::d3d);
  CHECK(named(d3d.command_name(av::d3dproto::kCmd_ClearState11), "ClearState11"));
  CHECK(named(d3d.command_name(av::d3d12proto::kCmd_DrawInstanced12), "DrawInstanced12"));
  CHECK(named(d3d.command_name(av::d3d9proto::kCmd_Clear9), "Clear9"));
  CHECK(named(d3d.command_name(0x0004FFFFu), "unknown")); }

TEST(command_api, the_other_apis_and_the_transport_name_their_commands)
{ CHECK(named(CommandApi(SubApi::vulkan).command_name(0x00010001u), "vkDestroyInstance"));
  CHECK(named(CommandApi(SubApi::opengl).command_name(av::glproto::kCmd_glActiveTexture), "glActiveTexture"));
  CHECK(named(CommandApi(SubApi::opencl).command_name(av::clproto::kCmd_clFinish), "clFinish"));
  const CommandApi transport(av::CommandHeader::transport_api);
  CHECK(named(transport.command_name(av::CommandHeader::nop_id), "nop"));
  CHECK(named(transport.command_name(av::CommandHeader::import_blob_id), "import_blob"));
  CHECK(named(transport.command_name(av::CommandHeader::release_blob_id), "release_blob"));
  CHECK(named(transport.command_name(0x77u), "unknown"));
  CHECK(named(CommandApi(std::uint16_t{9u}).command_name(1u), "unknown")); }
