#include "executor/gpu_address_registry.hpp"
#include "executor/vulkan/vulkan_addresses.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::GpuAddressRegistry;
using av::MockPlatform;
using av::VulkanAddresses;

namespace {

constexpr std::uint64_t device = 0x100u;
constexpr std::uint64_t vertices_base = 0x10000u;
constexpr std::uint64_t indices_base = 0x20000u;
constexpr std::uint64_t instances_base = 0x30000u;
constexpr std::uint64_t table_base = 0x40000u;

class Rig {
public:
    Rig() : addresses_(platform_)
    {
        add(1u, vertices_base, 4096u);
        add(2u, indices_base, 4096u);
        add(3u, instances_base, 4096u);
        add(4u, table_base, 4096u);
    }

    VulkanAddresses &addresses() { return addresses_; }

private:
    void add(std::uint64_t buffer, std::uint64_t address, std::uint64_t bytes)
    { CHECK(addresses_.note_buffer(device, buffer, bytes));
      CHECK(addresses_.note_address(device, buffer, address)); }

    MockPlatform    platform_;
    VulkanAddresses addresses_;
};

VkAccelerationStructureGeometryKHR triangles(std::uint64_t vertices, std::uint64_t indices)
{ VkAccelerationStructureGeometryKHR geometry = {};
  geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
  geometry.geometry.triangles.vertexData.deviceAddress = vertices;
  geometry.geometry.triangles.vertexStride = 12u;
  geometry.geometry.triangles.maxVertex = 8u;
  geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT16;
  geometry.geometry.triangles.indexData.deviceAddress = indices;
  return geometry; }

VkAccelerationStructureGeometryKHR instances(std::uint64_t address)
{ VkAccelerationStructureGeometryKHR geometry = {};
  geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
  geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
  geometry.geometry.instances.data.deviceAddress = address;
  return geometry; }

}

TEST(gpu_address_registry, a_range_covers_what_is_inside_it_and_nothing_across_its_end_or_on_another_device)
{ MockPlatform platform;
  GpuAddressRegistry registry(platform);
  CHECK_STATUS(registry.learn(1u, 10u, 0x1000u, 0x100u), av::Status::ok);
  CHECK(registry.covers(1u, 0x1000u, 0x100u));
  CHECK(registry.covers(1u, 0x10FFu, 1u));
  CHECK(!registry.covers(1u, 0x10FFu, 2u));
  CHECK(!registry.covers(1u, 0x1100u, 1u));
  CHECK(!registry.covers(1u, 0xFFFu, 1u));
  CHECK(!registry.covers(2u, 0x1000u, 1u));
  CHECK(!registry.covers(1u, 0x1000u, ~std::uint64_t{0}));
  CHECK_EQ(registry.room_after(1u, 0x1080u), std::uint64_t{0x80u});
  CHECK(registry.invariants_hold()); }

TEST(gpu_address_registry, overlapping_ranges_are_refused_and_forgetting_an_owner_or_a_device_removes_its_ranges)
{ MockPlatform platform;
  GpuAddressRegistry registry(platform);
  CHECK_STATUS(registry.learn(1u, 10u, 0x1000u, 0x100u), av::Status::ok);
  CHECK_STATUS(registry.learn(1u, 11u, 0x1080u, 0x100u), av::Status::invalid_state);
  CHECK_STATUS(registry.learn(1u, 11u, 0xF80u, 0x100u), av::Status::invalid_state);
  CHECK_STATUS(registry.learn(1u, 12u, 0x2000u, 0x100u), av::Status::ok);
  CHECK_STATUS(registry.learn(2u, 13u, 0x3000u, 0x100u), av::Status::ok);
  CHECK_STATUS(registry.learn(1u, 14u, 0u, 0x100u), av::Status::invalid_parameter);
  CHECK_STATUS(registry.learn(1u, 14u, 0x5000u, 0u), av::Status::invalid_parameter);
  CHECK_STATUS(registry.learn(1u, 14u, ~std::uint64_t{0}, 0x100u), av::Status::invalid_parameter);
  CHECK_EQ(registry.count(), 3u);
  registry.forget_owner(10u);
  CHECK(!registry.covers(1u, 0x1000u, 1u));
  registry.forget_device(1u);
  CHECK_EQ(registry.count(), 1u);
  CHECK(registry.covers(2u, 0x3000u, 1u));
  CHECK(registry.invariants_hold()); }

TEST(vulkan_addresses, a_build_whose_vertices_and_indices_lie_inside_the_device_s_buffers_is_admitted)
{ Rig rig;
  const VkAccelerationStructureBuildRangeInfoKHR range = {2u, 0u, 0u, 0u};
  CHECK(rig.addresses().geometry_admitted(device, triangles(vertices_base, indices_base), range, 2u)); }

TEST(vulkan_addresses, a_forged_vertex_index_or_instance_address_is_refused)
{ Rig rig;
  const VkAccelerationStructureBuildRangeInfoKHR range = {2u, 0u, 0u, 0u};
  CHECK(!rig.addresses().geometry_admitted(device, triangles(0xDEAD0000u, indices_base), range, 2u));
  CHECK(!rig.addresses().geometry_admitted(device, triangles(vertices_base, 0xDEAD0000u), range, 2u));
  CHECK(!rig.addresses().geometry_admitted(device, triangles(0u, indices_base), range, 2u));
  CHECK(!rig.addresses().geometry_admitted(device + 1u, triangles(vertices_base, indices_base), range, 2u));
  const VkAccelerationStructureBuildRangeInfoKHR one = {1u, 0u, 0u, 0u};
  CHECK(rig.addresses().geometry_admitted(device, instances(instances_base), one, 1u));
  CHECK(!rig.addresses().geometry_admitted(device, instances(0xBAD00000u), one, 1u));
  CHECK(!rig.addresses().geometry_admitted(device, instances(instances_base + 4090u), one, 1u)); }

TEST(vulkan_addresses, a_giant_primitive_count_reads_past_its_buffer_and_is_refused)
{ Rig rig;
  const VkAccelerationStructureBuildRangeInfoKHR giant = {0x7FFFFFFFu, 0u, 0u, 0u};
  CHECK(!rig.addresses().geometry_admitted(device, triangles(vertices_base, indices_base), giant, 0x7FFFFFFFu));
  CHECK(!rig.addresses().geometry_admitted(device, instances(instances_base), giant, 0x7FFFFFFFu));
  VkAccelerationStructureGeometryKHR aabbs = {};
  aabbs.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
  aabbs.geometry.aabbs.data.deviceAddress = vertices_base;
  aabbs.geometry.aabbs.stride = 24u;
  CHECK(!rig.addresses().geometry_admitted(device, aabbs, giant, 0xFFFFFFFFu));
  const VkAccelerationStructureBuildRangeInfoKHR sane = {16u, 0u, 0u, 0u};
  CHECK(rig.addresses().geometry_admitted(device, aabbs, sane, 16u));
  VkAccelerationStructureGeometryKHR unknown = {};
  unknown.geometryType = static_cast<VkGeometryTypeKHR>(0x7FFF);
  CHECK(!rig.addresses().geometry_admitted(device, unknown, sane, 1u)); }

TEST(vulkan_addresses, a_shader_binding_table_outside_every_buffer_or_a_trace_too_large_is_refused)
{ Rig rig;
  VkStridedDeviceAddressRegionKHR regions[4] = {};
  regions[0] = {table_base, 64u, 64u};
  regions[1] = {table_base + 64u, 64u, 64u};
  CHECK(rig.addresses().trace_admitted(device, regions, 64u, 64u, 1u));
  regions[2] = {0xFFFF0000u, 64u, 64u};
  CHECK(!rig.addresses().trace_admitted(device, regions, 64u, 64u, 1u));
  regions[2] = {table_base + 4090u, 64u, 64u};
  CHECK(!rig.addresses().trace_admitted(device, regions, 64u, 64u, 1u));
  regions[2] = {};
  VulkanAddresses::Limits limits = rig.addresses().limits();
  limits.dispatch_ = 1000u;
  rig.addresses().limits(limits);
  CHECK(rig.addresses().trace_admitted(device, regions, 10u, 10u, 10u));
  CHECK(!rig.addresses().trace_admitted(device, regions, 10u, 10u, 11u));
  CHECK(!rig.addresses().trace_admitted(device, regions, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu));
  regions[3] = {0u, 0u, 8u};
  CHECK(rig.addresses().trace_admitted(device, regions, 1u, 1u, 1u)); }

TEST(vulkan_addresses, a_descriptor_naming_an_address_outside_every_buffer_is_refused_and_a_null_one_passes)
{ Rig rig;
  VkDescriptorAddressInfoEXT inside = {};
  inside.address = vertices_base;
  inside.range = 256u;
  VkDescriptorAddressInfoEXT outside = inside;
  outside.address = 0xDEAD0000u;
  VkDescriptorAddressInfoEXT too_long = inside;
  too_long.range = 1u << 20u;
  VkDescriptorAddressInfoEXT none = {};
  VkDescriptorGetInfoEXT info = {};
  info.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  info.data.pUniformBuffer = &inside;
  CHECK(rig.addresses().descriptor_admitted(device, info));
  info.data.pUniformBuffer = &outside;
  CHECK(!rig.addresses().descriptor_admitted(device, info));
  info.data.pUniformBuffer = &too_long;
  CHECK(!rig.addresses().descriptor_admitted(device, info));
  info.data.pUniformBuffer = &none;
  CHECK(rig.addresses().descriptor_admitted(device, info));
  info.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
  info.data.accelerationStructure = 0xBAD00000u;
  CHECK(!rig.addresses().descriptor_admitted(device, info));
  info.data.accelerationStructure = instances_base;
  CHECK(rig.addresses().descriptor_admitted(device, info));
  info.type = VK_DESCRIPTOR_TYPE_SAMPLER;
  CHECK(rig.addresses().descriptor_admitted(device, info)); }

TEST(vulkan_addresses, destroying_a_buffer_takes_its_address_away_and_a_structure_belongs_to_its_device)
{ Rig rig;
  CHECK(rig.addresses().covers(device, vertices_base, 16u));
  rig.addresses().forget_buffer(1u);
  CHECK(!rig.addresses().covers(device, vertices_base, 16u));
  CHECK(rig.addresses().note_structure(device, 0x77u, 1024u));
  std::uint64_t bytes = 0u;
  CHECK(rig.addresses().structure_bytes(device, 0x77u, bytes));
  CHECK_EQ(bytes, std::uint64_t{1024u});
  CHECK(!rig.addresses().structure_bytes(device + 1u, 0x77u, bytes));
  rig.addresses().forget_device(device);
  CHECK(!rig.addresses().structure_bytes(device, 0x77u, bytes));
  CHECK_EQ(rig.addresses().ranges(), 0u);
  CHECK(rig.addresses().invariants_hold()); }

TEST(vulkan_addresses, generated_commands_are_admitted_only_with_every_address_inside_a_buffer_of_the_device)
{ Rig rig;
  VkGeneratedCommandsInfoEXT info = {};
  info.indirectAddress = vertices_base;
  info.indirectAddressSize = 64u;
  info.maxSequenceCount = 4u;
  CHECK(rig.addresses().generated_admitted(device, info));
  CHECK(!rig.addresses().generated_admitted(device + 1u, info));
  VkGeneratedCommandsInfoEXT none = info;
  none.maxSequenceCount = 0u;
  CHECK(!rig.addresses().generated_admitted(device, none));
  VkGeneratedCommandsInfoEXT empty = info;
  empty.indirectAddressSize = 0u;
  CHECK(!rig.addresses().generated_admitted(device, empty));
  VkGeneratedCommandsInfoEXT outside = info;
  outside.indirectAddress = 0xDEAD0000u;
  CHECK(!rig.addresses().generated_admitted(device, outside));
  VkGeneratedCommandsInfoEXT long_read = info;
  long_read.indirectAddressSize = 1u << 20u;
  CHECK(!rig.addresses().generated_admitted(device, long_read));
  VkGeneratedCommandsInfoEXT preprocessed = info;
  preprocessed.preprocessAddress = vertices_base;
  preprocessed.preprocessSize = 32u;
  CHECK(rig.addresses().generated_admitted(device, preprocessed));
  preprocessed.preprocessSize = 0u;
  CHECK(!rig.addresses().generated_admitted(device, preprocessed));
  preprocessed.preprocessAddress = 0xDEAD0000u;
  preprocessed.preprocessSize = 32u;
  CHECK(!rig.addresses().generated_admitted(device, preprocessed));
  VkGeneratedCommandsInfoEXT counted = info;
  counted.sequenceCountAddress = vertices_base;
  CHECK(rig.addresses().generated_admitted(device, counted));
  counted.sequenceCountAddress = 0xDEAD0000u;
  CHECK(!rig.addresses().generated_admitted(device, counted)); }
