#include "core/log/driver_log.hpp"
#include "executor/vulkan/vulkan_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstring>

namespace {

struct ShaderDriver {
    std::uint32_t modules = 0u;
    std::uint32_t graphics = 0u;
    std::uint32_t compute = 0u;
    std::uint32_t ray_tracing = 0u;
};

ShaderDriver driver;

const std::uint32_t compute_nop[] = {
    0x07230203u, 0x00010600u, 0x00070000u, 0x00000005u, 0x00000000u, 0x00020011u, 0x00000001u, 0x0003000eu,
    0x00000000u, 0x00000001u, 0x0005000fu, 0x00000005u, 0x00000001u, 0x6e69616du, 0x00000000u, 0x00060010u,
    0x00000001u, 0x00000011u, 0x00000001u, 0x00000001u, 0x00000001u, 0x00020013u, 0x00000002u, 0x00030021u,
    0x00000003u, 0x00000002u, 0x00050036u, 0x00000002u, 0x00000001u, 0x00000000u, 0x00000003u, 0x000200f8u,
    0x00000004u, 0x000100fdu, 0x00010038u};

class Rig {
public:
    Rig() : sink_(), lines_(sink_, "vulkan: "), backend_(lines_) { driver = ShaderDriver{}; }
    ~Rig() { backend_.shut_down(); }

    av::VulkanBackend &backend() { return backend_; }
    VkDevice           device() const { return reinterpret_cast<VkDevice>(std::uintptr_t{0x300}); }

private:
    av::RecordingLog sink_;
    av::DriverLog    lines_;
    av::VulkanBackend backend_;
};

VkShaderModuleCreateInfo module_of(const std::uint32_t *words, std::size_t bytes)
{ VkShaderModuleCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create.codeSize = bytes;
  create.pCode = words;
  return create; }

}

extern "C" {

VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(VkDevice, const VkShaderModuleCreateInfo *,
                                                    const VkAllocationCallbacks *, VkShaderModule *module)
{
    driver.modules++;
    *module = reinterpret_cast<VkShaderModule>(std::uintptr_t{0x900} + driver.modules);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(VkDevice, VkPipelineCache, uint32_t count,
                                                         const VkGraphicsPipelineCreateInfo *,
                                                         const VkAllocationCallbacks *, VkPipeline *pipelines)
{
    driver.graphics++;
    for (uint32_t at = 0u; at < count; at++) pipelines[at] = reinterpret_cast<VkPipeline>(std::uintptr_t{0xA00} + at);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateComputePipelines(VkDevice, VkPipelineCache, uint32_t count,
                                                        const VkComputePipelineCreateInfo *,
                                                        const VkAllocationCallbacks *, VkPipeline *pipelines)
{
    driver.compute++;
    for (uint32_t at = 0u; at < count; at++) pipelines[at] = reinterpret_cast<VkPipeline>(std::uintptr_t{0xB00} + at);
    return VK_SUCCESS;
}

}

TEST(vulkan_shader_validation, a_valid_module_reaches_the_driver)
{ Rig rig;
  const VkShaderModuleCreateInfo create = module_of(compute_nop, sizeof compute_nop);
  VkShaderModule made = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(rig.backend().on_vkCreateShaderModule(rig.device(), &create, nullptr, &made)),
           static_cast<int>(VK_SUCCESS));
  CHECK(made != VK_NULL_HANDLE);
  CHECK_EQ(driver.modules, 1u); }

TEST(vulkan_shader_validation, a_corrupt_module_never_reaches_the_driver_and_leaves_no_handle)
{ Rig rig;
  std::uint32_t bad[sizeof compute_nop / 4u];
  std::memcpy(bad, compute_nop, sizeof bad);
  bad[0] = 0u;
  const VkShaderModuleCreateInfo create = module_of(bad, sizeof bad);
  VkShaderModule made = reinterpret_cast<VkShaderModule>(std::uintptr_t{1});
  CHECK_EQ(static_cast<int>(rig.backend().on_vkCreateShaderModule(rig.device(), &create, nullptr, &made)),
           static_cast<int>(VK_ERROR_UNKNOWN));
  CHECK(made == VK_NULL_HANDLE);
  CHECK_EQ(driver.modules, 0u); }

TEST(vulkan_shader_validation, a_compute_pipeline_with_an_embedded_corrupt_module_is_refused_whole)
{ Rig rig;
  std::uint32_t bad[sizeof compute_nop / 4u];
  std::memcpy(bad, compute_nop, sizeof bad);
  bad[5] = 0x00FF0011u;
  VkShaderModuleCreateInfo embedded = module_of(bad, sizeof bad);
  VkComputePipelineCreateInfo create[2] = {};
  create[0].sType = create[1].sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  const VkShaderModuleCreateInfo good = module_of(compute_nop, sizeof compute_nop);
  create[0].stage.pNext = &good;
  create[1].stage.pNext = &embedded;
  VkPipeline made[2] = {reinterpret_cast<VkPipeline>(std::uintptr_t{1}), reinterpret_cast<VkPipeline>(std::uintptr_t{2})};
  CHECK_EQ(static_cast<int>(rig.backend().on_vkCreateComputePipelines(rig.device(), VK_NULL_HANDLE, 2u, create, nullptr, made)),
           static_cast<int>(VK_ERROR_UNKNOWN));
  CHECK(made[0] == VK_NULL_HANDLE);
  CHECK(made[1] == VK_NULL_HANDLE);
  CHECK_EQ(driver.compute, 0u); }

TEST(vulkan_shader_validation, a_graphics_pipeline_of_valid_embedded_modules_reaches_the_driver)
{ Rig rig;
  const VkShaderModuleCreateInfo good = module_of(compute_nop, sizeof compute_nop);
  VkPipelineShaderStageCreateInfo stages[2] = {};
  stages[0].pNext = &good;
  stages[1].module = reinterpret_cast<VkShaderModule>(std::uintptr_t{0x77});
  VkGraphicsPipelineCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  create.stageCount = 2u;
  create.pStages = stages;
  VkPipeline made = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(rig.backend().on_vkCreateGraphicsPipelines(rig.device(), VK_NULL_HANDLE, 1u, &create, nullptr, &made)),
           static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.graphics, 1u);
  CHECK(made != VK_NULL_HANDLE); }

TEST(vulkan_shader_validation, a_graphics_pipeline_with_a_corrupt_embedded_module_is_refused)
{ Rig rig;
  std::uint32_t bad[sizeof compute_nop / 4u];
  std::memcpy(bad, compute_nop, sizeof bad);
  bad[0] = 0u;
  const VkShaderModuleCreateInfo embedded = module_of(bad, sizeof bad);
  VkPipelineShaderStageCreateInfo stage = {};
  stage.pNext = &embedded;
  VkGraphicsPipelineCreateInfo create = {};
  create.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  create.stageCount = 1u;
  create.pStages = &stage;
  VkPipeline made = reinterpret_cast<VkPipeline>(std::uintptr_t{1});
  CHECK_EQ(static_cast<int>(rig.backend().on_vkCreateGraphicsPipelines(rig.device(), VK_NULL_HANDLE, 1u, &create, nullptr, &made)),
           static_cast<int>(VK_ERROR_UNKNOWN));
  CHECK(made == VK_NULL_HANDLE);
  CHECK_EQ(driver.graphics, 0u); }
