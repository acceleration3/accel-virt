#include "core/log/driver_log.hpp"
#include "executor/vulkan/issued_binaries.hpp"
#include "executor/vulkan/vulkan_backend.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstring>

namespace {

struct BinaryDriver {
    std::uint32_t shader_creates = 0u;
    std::uint32_t pipeline_creates = 0u;
};

BinaryDriver driver;

constexpr std::size_t blob_bytes = 48u;

void fill_blob(void *out)
{ auto *bytes = static_cast<std::uint8_t *>(out);
  for (std::size_t at = 0u; at < blob_bytes; at++) bytes[at] = static_cast<std::uint8_t>(at * 7u + 3u); }

class Rig {
public:
    Rig() : sink_(), lines_(sink_, "vulkan: "), backend_(lines_) { driver = BinaryDriver{}; }
    ~Rig() { backend_.shut_down(); }

    av::VulkanBackend &backend() { return backend_; }
    VkDevice           device() const { return reinterpret_cast<VkDevice>(std::uintptr_t{0x300}); }
    VkShaderEXT        shader() const { return reinterpret_cast<VkShaderEXT>(std::uintptr_t{0x500}); }

    VkResult create(const void *code, std::size_t bytes, VkShaderEXT &made)
    { VkShaderCreateInfoEXT info = {};
      info.sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
      info.codeType = VK_SHADER_CODE_TYPE_BINARY_EXT;
      info.codeSize = bytes;
      info.pCode = code;
      made = reinterpret_cast<VkShaderEXT>(std::uintptr_t{1});
      return backend_.on_vkCreateShadersEXT(device(), 1u, &info, nullptr, &made); }

    void hand_out(std::uint8_t (&blob)[blob_bytes])
    { std::size_t size = blob_bytes;
      CHECK_EQ(static_cast<int>(backend_.on_vkGetShaderBinaryDataEXT(device(), shader(), &size, blob)),
               static_cast<int>(VK_SUCCESS)); }

private:
    av::RecordingLog  sink_;
    av::DriverLog     lines_;
    av::VulkanBackend backend_;
};

VkResult VKAPI_CALL stub_create_shaders(VkDevice, uint32_t count, const VkShaderCreateInfoEXT *, const VkAllocationCallbacks *,
                                        VkShaderEXT *shaders)
{
    driver.shader_creates++;
    for (uint32_t at = 0u; at < count; at++) shaders[at] = reinterpret_cast<VkShaderEXT>(std::uintptr_t{0x700} + at);
    return VK_SUCCESS;
}

VkResult VKAPI_CALL stub_shader_data(VkDevice, VkShaderEXT, size_t *size, void *data)
{
    if (data != nullptr) fill_blob(data);
    *size = blob_bytes;
    return VK_SUCCESS;
}

VkResult VKAPI_CALL stub_create_binaries(VkDevice, const VkPipelineBinaryCreateInfoKHR *, const VkAllocationCallbacks *,
                                         VkPipelineBinaryHandlesInfoKHR *handles)
{
    driver.pipeline_creates++;
    for (uint32_t at = 0u; at < handles->pipelineBinaryCount; at++)
        handles->pPipelineBinaries[at] = reinterpret_cast<VkPipelineBinaryKHR>(std::uintptr_t{0x800} + at);
    return VK_SUCCESS;
}

VkResult VKAPI_CALL stub_binary_data(VkDevice, const VkPipelineBinaryDataInfoKHR *, VkPipelineBinaryKeyKHR *key,
                                     size_t *size, void *data)
{
    key->keySize = 4u;
    std::memcpy(key->key, "key!", 4u);
    if (data != nullptr) fill_blob(data);
    *size = blob_bytes;
    return VK_SUCCESS;
}

}

extern "C" {

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance, const char *name)
{
    if (std::strcmp(name, "vkCreateShadersEXT") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&stub_create_shaders);
    if (std::strcmp(name, "vkGetShaderBinaryDataEXT") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&stub_shader_data);
    if (std::strcmp(name, "vkCreatePipelineBinariesKHR") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(&stub_create_binaries);
    if (std::strcmp(name, "vkGetPipelineBinaryDataKHR") == 0)
        return reinterpret_cast<PFN_vkVoidFunction>(&stub_binary_data);
    return nullptr;
}

}

TEST(vulkan_binaries, a_shader_binary_the_host_did_not_hand_out_is_refused)
{ Rig rig;
  std::uint8_t foreign[blob_bytes];
  fill_blob(foreign);
  VkShaderEXT made = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(rig.create(foreign, blob_bytes, made)), static_cast<int>(VK_INCOMPATIBLE_SHADER_BINARY_EXT));
  CHECK(made == VK_NULL_HANDLE);
  CHECK_EQ(driver.shader_creates, 0u); }

TEST(vulkan_binaries, a_shader_binary_the_host_handed_out_is_accepted_and_nothing_altered_or_cut_short_is)
{ Rig rig;
  std::uint8_t blob[blob_bytes] = {};
  rig.hand_out(blob);
  VkShaderEXT made = VK_NULL_HANDLE;
  CHECK_EQ(static_cast<int>(rig.create(blob, blob_bytes, made)), static_cast<int>(VK_SUCCESS));
  CHECK(made != VK_NULL_HANDLE);
  CHECK_EQ(driver.shader_creates, 1u);
  CHECK_EQ(static_cast<int>(rig.create(blob, blob_bytes - 1u, made)), static_cast<int>(VK_INCOMPATIBLE_SHADER_BINARY_EXT));
  CHECK_EQ(static_cast<int>(rig.create(blob, 0u, made)), static_cast<int>(VK_INCOMPATIBLE_SHADER_BINARY_EXT));
  std::uint8_t longer[blob_bytes + 8u] = {};
  std::memcpy(longer, blob, blob_bytes);
  CHECK_EQ(static_cast<int>(rig.create(longer, sizeof longer, made)), static_cast<int>(VK_INCOMPATIBLE_SHADER_BINARY_EXT));
  for (std::size_t at = 0u; at < blob_bytes; at++) {
      std::uint8_t flipped[blob_bytes];
      std::memcpy(flipped, blob, blob_bytes);
      flipped[at] ^= 0x01u;
      CHECK_EQ(static_cast<int>(rig.create(flipped, blob_bytes, made)), static_cast<int>(VK_INCOMPATIBLE_SHADER_BINARY_EXT));
  }
  CHECK_EQ(driver.shader_creates, 1u); }

TEST(vulkan_binaries, a_pipeline_binary_the_host_did_not_hand_out_is_refused)
{ Rig rig;
  std::uint8_t foreign[blob_bytes];
  fill_blob(foreign);
  VkPipelineBinaryKeyKHR key = {};
  key.keySize = 4u;
  std::memcpy(key.key, "key!", 4u);
  VkPipelineBinaryDataKHR data = {blob_bytes, foreign};
  VkPipelineBinaryKeysAndDataKHR given = {1u, &key, &data};
  VkPipelineBinaryCreateInfoKHR create = {};
  create.sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_CREATE_INFO_KHR;
  create.pKeysAndDataInfo = &given;
  VkPipelineBinaryKHR binaries[1] = {reinterpret_cast<VkPipelineBinaryKHR>(std::uintptr_t{1})};
  VkPipelineBinaryHandlesInfoKHR handles = {};
  handles.sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_HANDLES_INFO_KHR;
  handles.pipelineBinaryCount = 1u;
  handles.pPipelineBinaries = binaries;
  CHECK_EQ(static_cast<int>(rig.backend().on_vkCreatePipelineBinariesKHR(rig.device(), &create, nullptr, &handles)),
           static_cast<int>(VK_ERROR_INITIALIZATION_FAILED));
  CHECK(binaries[0] == VK_NULL_HANDLE);
  CHECK_EQ(driver.pipeline_creates, 0u);

  std::uint8_t   handed[blob_bytes] = {};
  VkPipelineBinaryKeyKHR handed_key = {};
  std::size_t    size = blob_bytes;
  VkPipelineBinaryDataInfoKHR info = {};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_DATA_INFO_KHR;
  CHECK_EQ(static_cast<int>(rig.backend().on_vkGetPipelineBinaryDataKHR(rig.device(), &info, &handed_key, &size, handed)),
           static_cast<int>(VK_SUCCESS));
  data = {blob_bytes, handed};
  key = handed_key;
  CHECK_EQ(static_cast<int>(rig.backend().on_vkCreatePipelineBinariesKHR(rig.device(), &create, nullptr, &handles)),
           static_cast<int>(VK_SUCCESS));
  CHECK_EQ(driver.pipeline_creates, 1u);
  key.key[0] ^= 1u;
  CHECK_EQ(static_cast<int>(rig.backend().on_vkCreatePipelineBinariesKHR(rig.device(), &create, nullptr, &handles)),
           static_cast<int>(VK_ERROR_INITIALIZATION_FAILED));
  key = handed_key;
  data = {blob_bytes - 4u, handed};
  CHECK_EQ(static_cast<int>(rig.backend().on_vkCreatePipelineBinariesKHR(rig.device(), &create, nullptr, &handles)),
           static_cast<int>(VK_ERROR_INITIALIZATION_FAILED));
  CHECK_EQ(driver.pipeline_creates, 1u); }

TEST(vulkan_binaries, a_shader_binary_is_not_a_pipeline_binary_and_a_full_registry_forgets_its_oldest)
{ av::IssuedBinaries issued(1u, 2u);
  std::uint8_t blob[blob_bytes];
  fill_blob(blob);
  issued.issue(av::IssuedBinaries::Kind::shader, blob, blob_bytes);
  CHECK(issued.issued(av::IssuedBinaries::Kind::shader, blob, blob_bytes));
  CHECK(!issued.issued(av::IssuedBinaries::Kind::pipeline, blob, blob_bytes));
  CHECK(!issued.issued(av::IssuedBinaries::Kind::shader, nullptr, blob_bytes));
  for (std::uint32_t at = 0u; at < av::IssuedBinaries::capacity; at++) {
      std::uint8_t other[blob_bytes];
      fill_blob(other);
      std::memcpy(other, &at, sizeof at);
      other[sizeof at] = 0xEEu;
      issued.issue(av::IssuedBinaries::Kind::shader, other, blob_bytes);
  }
  CHECK_EQ(issued.count(), av::IssuedBinaries::capacity);
  CHECK(!issued.issued(av::IssuedBinaries::Kind::shader, blob, blob_bytes));
  CHECK(issued.invariants_hold());
  CHECK(issued.refused() > 0u); }
