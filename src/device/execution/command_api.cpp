#include "device/execution/command_api.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/d3d/d3d_command_names.h"
#include "proto/generated/d3d12/d3d12_command_names.h"
#include "proto/generated/d3d9/d3d9_command_names.h"
#include "proto/generated/opencl/cl_command_names.h"
#include "proto/generated/opengl/gl_command_names.h"
#include "proto/generated/vulkan/vk_command_names.h"
#include "proto/submission/command_header.hpp"

namespace av {

namespace {

constexpr uint16_t vulkan_commands = 1u;
constexpr uint16_t opencl_commands = 2u;
constexpr uint16_t opengl_commands = 3u;
constexpr uint16_t d3d_commands = 4u;
constexpr const char *unknown = "unknown";

struct ApiRow {
    uint16_t    api;
    uint32_t    sub_api;
    const char *name;
};

constexpr ApiRow rows[] = {
    {CommandHeader::transport_api, 0u, "transport"},
    {vulkan_commands, static_cast<uint32_t>(SubApi::vulkan), "vulkan"},
    {opencl_commands, static_cast<uint32_t>(SubApi::opencl), "opencl"},
    {opengl_commands, static_cast<uint32_t>(SubApi::opengl), "opengl"},
    {d3d_commands, static_cast<uint32_t>(SubApi::d3d), "d3d"},
};

}

CommandApi::CommandApi(uint16_t api) : api_(api)
{
    AV_SCOPE("CommandApi::CommandApi");
}

CommandApi::CommandApi(SubApi sub_api) : api_(none)
{
    AV_SCOPE("CommandApi::CommandApi");
    for (const ApiRow &row : rows)
        if (row.sub_api != 0u && row.sub_api == static_cast<uint32_t>(sub_api)) api_ = row.api;
}

bool CommandApi::take_name(const char *name)
{
    AV_SCOPE("CommandApi::take_name");
    if (name == nullptr) return false;
    for (const ApiRow &row : rows) {
        size_t at = 0u;
        while (row.name[at] != '\0' && row.name[at] == name[at]) at++;
        if (row.name[at] != name[at]) continue;
        api_ = row.api;
        return true;
    }
    return false;
}

uint16_t CommandApi::api() const
{
    AV_SCOPE("CommandApi::api");
    return api_;
}

bool CommandApi::known() const
{
    AV_SCOPE("CommandApi::known");
    for (const ApiRow &row : rows)
        if (row.api == api_) return true;
    return false;
}

const char *CommandApi::name() const
{
    AV_SCOPE("CommandApi::name");
    for (const ApiRow &row : rows)
        if (row.api == api_) return row.name;
    return "api";
}

const char *CommandApi::command_name(uint32_t id) const
{
    AV_SCOPE("CommandApi::command_name");
    switch (api_) {
    case CommandHeader::transport_api:
        if (id == CommandHeader::nop_id) return "nop";
        if (id == CommandHeader::import_blob_id) return "import_blob";
        if (id == CommandHeader::release_blob_id) return "release_blob";
        return unknown;
    case vulkan_commands: return vkproto::cmd_id_to_string(id);
    case opencl_commands: return clproto::cmd_id_to_string(id);
    case opengl_commands: return glproto::cmd_id_to_string(id);
    case d3d_commands: {
        const char *named = d3dproto::command_name(id);
        if (named == nullptr) named = d3d12proto::command_name(id);
        if (named == nullptr) named = d3d9proto::command_name(id);
        return named != nullptr ? named : unknown;
    }
    default: return unknown;
    }
}

}
