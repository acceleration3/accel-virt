#include "device/executor/vulkan_backend.hpp"

#include "proto/generated/vulkan/vk_command_names.h"
#include "proto/generated/vulkan/vk_dispatch_server.h"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <string.h>

namespace av {

VulkanBackend::VulkanBackend() : ran_(0u), lost_(false) {}

bool VulkanBackend::knows(uint32_t command_id) const
{
    if (command_id >> 16u != vulkan_api) return false;
    /* The generator writes one name per id it carries and "unknown" for the
     * rest, which is exactly the question being asked. Deciding it here rather
     * than by letting the dispatch fall off its switch keeps an id the profile
     * does not carry a skip instead of a fault. */
    return strcmp(vkproto::cmd_id_to_string(command_id), "unknown") != 0;
}

bool VulkanBackend::run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                        WireArena &arena)
{
    const bool decoded = vkproto::dispatch_command(*this, command_id, payload, reply, arena);
    if (!decoded) return false;
    ran_++;
    return true;
}

bool VulkanBackend::lost() const { return lost_; }

uint64_t VulkanBackend::ran() const { return ran_; }

}  // namespace av
