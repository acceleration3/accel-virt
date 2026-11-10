#include "executor/vulkan/vulkan_addresses.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

uint64_t index_bytes(VkIndexType type)
{
    AV_SCOPE("index_bytes");
    switch (type) {
    case VK_INDEX_TYPE_UINT16: return 2u;
    case VK_INDEX_TYPE_UINT32: return 4u;
    case VK_INDEX_TYPE_UINT8: return 1u;
    default: return 0u;
    }
}

bool multiplied(uint64_t first, uint64_t second, uint64_t &out)
{
    AV_SCOPE("multiplied");
    return !__builtin_mul_overflow(first, second, &out);
}

bool added(uint64_t first, uint64_t second, uint64_t &out)
{
    AV_SCOPE("added");
    return !__builtin_add_overflow(first, second, &out);
}

}

VulkanAddresses::VulkanAddresses(BlockSource &blocks)
    : registry_(blocks), buffers_(blocks, Pool::paged), structures_(blocks, Pool::paged),
      limits_{1u << 24u, 1u << 12u, 1u << 24u, 1u << 30u}
{
    AV_SCOPE("VulkanAddresses::VulkanAddresses");
}

void VulkanAddresses::limits(const Limits &limits)
{
    AV_SCOPE("VulkanAddresses::limits");
    limits_ = limits;
}

const VulkanAddresses::Limits &VulkanAddresses::limits() const
{
    AV_SCOPE("VulkanAddresses::limits");
    return limits_;
}

bool VulkanAddresses::note_buffer(uint64_t device, uint64_t buffer, uint64_t bytes)
{
    AV_SCOPE("VulkanAddresses::note_buffer");
    if (buffer == 0u) return false;
    const Buffer row{device, bytes};
    Buffer *const known = buffers_.find(buffer);
    if (known != nullptr) {
        *known = row;
        return true;
    }
    uint32_t slot = 0u;
    return buffers_.insert(buffer, row, slot) == Status::ok;
}

bool VulkanAddresses::note_address(uint64_t device, uint64_t buffer, uint64_t address)
{
    AV_SCOPE("VulkanAddresses::note_address");
    const Buffer *const known = buffers_.find(buffer);
    if (known == nullptr || known->device_ != device || address == 0u) return false;
    return registry_.learn(device, buffer, address, known->bytes_) == Status::ok;
}

void VulkanAddresses::forget_buffer(uint64_t buffer)
{
    AV_SCOPE("VulkanAddresses::forget_buffer");
    if (!buffers_.erase(buffer)) return;
    registry_.forget_owner(buffer);
}

bool VulkanAddresses::buffer_bytes(uint64_t device, uint64_t buffer, uint64_t &bytes) const
{
    AV_SCOPE("VulkanAddresses::buffer_bytes");
    const Buffer *const known = buffers_.find(buffer);
    if (known == nullptr || known->device_ != device) return false;
    bytes = known->bytes_;
    return true;
}

bool VulkanAddresses::note_structure(uint64_t device, uint64_t structure, uint64_t bytes)
{
    AV_SCOPE("VulkanAddresses::note_structure");
    if (structure == 0u) return false;
    const Structure row{device, bytes};
    Structure *const known = structures_.find(structure);
    if (known != nullptr) {
        *known = row;
        return true;
    }
    uint32_t slot = 0u;
    return structures_.insert(structure, row, slot) == Status::ok;
}

void VulkanAddresses::forget_structure(uint64_t structure)
{
    AV_SCOPE("VulkanAddresses::forget_structure");
    static_cast<void>(structures_.erase(structure));
}

bool VulkanAddresses::structure_bytes(uint64_t device, uint64_t structure, uint64_t &bytes) const
{
    AV_SCOPE("VulkanAddresses::structure_bytes");
    const Structure *const known = structures_.find(structure);
    if (known == nullptr || known->device_ != device) return false;
    bytes = known->bytes_;
    return true;
}

void VulkanAddresses::forget_device(uint64_t device)
{
    AV_SCOPE("VulkanAddresses::forget_device");
    registry_.forget_device(device);
    buffers_.erase_if([device](uint64_t, Buffer &row) { return row.device_ == device; });
    structures_.erase_if([device](uint64_t, Structure &row) { return row.device_ == device; });
}

bool VulkanAddresses::covers(uint64_t device, uint64_t address, uint64_t bytes) const
{
    AV_SCOPE("VulkanAddresses::covers");
    return registry_.covers(device, address, bytes);
}

bool VulkanAddresses::region_admitted(uint64_t device, const VkStridedDeviceAddressRegionKHR &region) const
{
    AV_SCOPE("VulkanAddresses::region_admitted");
    if (region.deviceAddress == 0u) return region.size == 0u;
    return region.size != 0u && covers(device, region.deviceAddress, region.size);
}

bool VulkanAddresses::trace_admitted(uint64_t device, const VkStridedDeviceAddressRegionKHR *regions, uint32_t width,
                                     uint32_t height, uint32_t depth) const
{
    AV_SCOPE("VulkanAddresses::trace_admitted");
    for (uint32_t at = 0u; at < 4u; at++)
        if (regions[at].deviceAddress != 0u && !region_admitted(device, regions[at])) return false;
    uint64_t invocations = 0u;
    return multiplied(width, height, invocations) && multiplied(invocations, depth, invocations) &&
           invocations <= limits_.dispatch_;
}

bool VulkanAddresses::geometry_admitted(uint64_t device, const VkAccelerationStructureGeometryKHR &geometry,
                                        const VkAccelerationStructureBuildRangeInfoKHR &range,
                                        uint32_t primitives) const
{
    AV_SCOPE("VulkanAddresses::geometry_admitted");
    uint64_t bytes = 0u;
    uint64_t start = 0u;
    switch (geometry.geometryType) {
    case VK_GEOMETRY_TYPE_TRIANGLES_KHR: {
        const VkAccelerationStructureGeometryTrianglesDataKHR &data = geometry.geometry.triangles;
        uint64_t vertices = 0u;
        if (!added(data.maxVertex, uint64_t{range.firstVertex} + 1u, vertices) ||
            !multiplied(vertices, data.vertexStride, bytes) || data.vertexData.deviceAddress == 0u ||
            !covers(device, data.vertexData.deviceAddress, bytes))
            return false;
        if (data.indexType != VK_INDEX_TYPE_NONE_KHR) {
            const uint64_t size = index_bytes(data.indexType);
            uint64_t       count = 0u;
            if (size == 0u || !multiplied(uint64_t{primitives}, 3u, count) || !multiplied(count, size, bytes) ||
                !added(data.indexData.deviceAddress, range.primitiveOffset, start) || data.indexData.deviceAddress == 0u ||
                !covers(device, start, bytes))
                return false;
        }
        if (data.transformData.deviceAddress != 0u) {
            if (!added(data.transformData.deviceAddress, range.transformOffset, start) ||
                !covers(device, start, sizeof(VkTransformMatrixKHR)))
                return false;
        }
        return true;
    }
    case VK_GEOMETRY_TYPE_AABBS_KHR: {
        const VkAccelerationStructureGeometryAabbsDataKHR &data = geometry.geometry.aabbs;
        if (!multiplied(uint64_t{primitives}, data.stride != 0u ? data.stride : sizeof(VkAabbPositionsKHR), bytes) ||
            data.data.deviceAddress == 0u || !added(data.data.deviceAddress, range.primitiveOffset, start))
            return false;
        return covers(device, start, bytes);
    }
    case VK_GEOMETRY_TYPE_INSTANCES_KHR: {
        const VkAccelerationStructureGeometryInstancesDataKHR &data = geometry.geometry.instances;
        const uint64_t each = data.arrayOfPointers != VK_FALSE ? sizeof(VkDeviceAddress)
                                                             : sizeof(VkAccelerationStructureInstanceKHR);
        if (!multiplied(uint64_t{primitives}, each, bytes) || data.data.deviceAddress == 0u ||
            !added(data.data.deviceAddress, range.primitiveOffset, start))
            return false;
        return covers(device, start, bytes);
    }
    default: return false;
    }
}

bool VulkanAddresses::address_info_admitted(uint64_t device, const VkDescriptorAddressInfoEXT *info) const
{
    AV_SCOPE("VulkanAddresses::address_info_admitted");
    if (info == nullptr || info->address == 0u) return true;
    return covers(device, info->address, info->range);
}

bool VulkanAddresses::descriptor_admitted(uint64_t device, const VkDescriptorGetInfoEXT &info) const
{
    AV_SCOPE("VulkanAddresses::descriptor_admitted");
    switch (info.type) {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return address_info_admitted(device, info.data.pUniformBuffer);
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: return address_info_admitted(device, info.data.pStorageBuffer);
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: return address_info_admitted(device, info.data.pUniformTexelBuffer);
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: return address_info_admitted(device, info.data.pStorageTexelBuffer);
    case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        return info.data.accelerationStructure == 0u || covers(device, info.data.accelerationStructure, 1u);
    default: return true;
    }
}

uint32_t VulkanAddresses::buffers() const
{
    AV_SCOPE("VulkanAddresses::buffers");
    return buffers_.count();
}

uint32_t VulkanAddresses::structures() const
{
    AV_SCOPE("VulkanAddresses::structures");
    return structures_.count();
}

uint32_t VulkanAddresses::ranges() const
{
    AV_SCOPE("VulkanAddresses::ranges");
    return registry_.count();
}

bool VulkanAddresses::invariants_hold() const
{
    AV_SCOPE("VulkanAddresses::invariants_hold");
    return registry_.invariants_hold() && buffers_.invariants_hold() && structures_.invariants_hold();
}

}
