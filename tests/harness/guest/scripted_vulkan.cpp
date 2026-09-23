#include "harness/guest/scripted_vulkan.hpp"

#include "guest/kmt/kmt.hpp"
#include "harness/guest/scripted_window_frame.hpp"

#include <cstring>

namespace av {

namespace {

constexpr std::uint64_t first_handle = 0x1000u;
constexpr std::uint64_t physical_device_base = 0x100u;
constexpr std::uint64_t queue_base = 0x200u;
constexpr std::uint64_t command_buffer_base = 0x300u;
constexpr std::uint32_t local_size_x = 16u;
constexpr std::uint32_t element_bytes = 4u;
constexpr std::uint32_t pixel_bytes = 4u;
constexpr VkDeviceSize  heap_bytes = VkDeviceSize{1} << 30u;

std::uint64_t raw(const void *handle) { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle)); }

template <typename Handle>
Handle as_handle(std::uint64_t value)
{
    return reinterpret_cast<Handle>(static_cast<std::uintptr_t>(value));
}

}  // namespace

ScriptedVulkan::ScriptedVulkan()
    : calls_{}, fail_at_{}, fail_with_{}, physical_devices_(1u), luids_{}, luid_valid_{}, queue_families_(1u),
      queue_flags_(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT), memory_{}, buffer_types_(0x3u), image_types_(0x3u),
      miscompute_(false), misdraw_(false), hold_fences_(false), fault_answer_(VK_ERROR_DEVICE_LOST),
      idle_after_fault_(VK_ERROR_DEVICE_LOST), luid_xor_(0u),
      flags_after_fault_(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT), lost_(false), objects_{}, misuses_(0u), next_handle_(first_handle),
      external_objects_(false), win32_handle_(nullptr), scripted_extensions_(false), extension_count_(0u),
      extension_first_(VK_SUCCESS), extension_second_(VK_SUCCESS), allocate_nothing_(false), no_handle_(false),
      release_code_(0u), kernel_status_(0u),
      allocations_{}, allocated_types_{}, allocations_made_(0u), recording_{}, api_version_(0u),
      dispatched_groups_(0u), drawn_vertices_(0u), shader_words_(0u), device_parent_(0u), window_(nullptr), window_handle_(nullptr), pacing_kmt_(nullptr),
      pacing_adapter_(0u), pacing_device_(0u), instance_names_{}, instance_name_count_(0u), device_names_{},
      device_name_count_(0u), held_extent_(false), nothing_presentable_(false), lost_surface_(false),
      lost_present_modes_(false), one_mode_(false), only_mode_(VK_PRESENT_MODE_FIFO_KHR),
      least_images_(images_per_swapchain), lost_swapchain_images_(false), one_image_(false),
      held_width_(0u), held_height_(0u), swapchain_(0u),
      swapchain_width_(0u), swapchain_height_(0u), swapchain_images_{}, retired_(0u), retired_images_{}, next_image_(0u), presents_(0u),
      drew_since_present_(false)
{
    for (std::uint32_t index = 0u; index < max_physical_devices; index++) {
        luids_[index] = index + 1u;
        luid_valid_[index] = true;
    }
    set_memory_types(default_memory_types);
    set_memory_type(0u, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    set_memory_type(1u, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void ScriptedVulkan::fail(Call call, std::uint32_t nth, VkResult result)
{
    fail_at_[static_cast<std::uint32_t>(call)] = nth;
    fail_with_[static_cast<std::uint32_t>(call)] = result;
}

std::uint32_t ScriptedVulkan::calls(Call call) const { return calls_[static_cast<std::uint32_t>(call)]; }

void ScriptedVulkan::set_physical_devices(std::uint32_t count) { physical_devices_ = count; }

void ScriptedVulkan::set_luid(std::uint32_t index, std::uint64_t luid, bool valid)
{
    luids_[index] = luid;
    luid_valid_[index] = valid;
}

void ScriptedVulkan::set_queue_families(std::uint32_t count, VkQueueFlags flags)
{
    queue_families_ = count;
    queue_flags_ = flags;
}

void ScriptedVulkan::set_memory_types(std::uint32_t count)
{
    memory_.memoryTypeCount = count;
    memory_.memoryHeapCount = 1u;
    memory_.memoryHeaps[0].size = heap_bytes;
}

void ScriptedVulkan::set_memory_type(std::uint32_t index, VkMemoryPropertyFlags flags)
{
    memory_.memoryTypes[index].propertyFlags = flags;
    memory_.memoryTypes[index].heapIndex = 0u;
}

void ScriptedVulkan::set_allowed_types(std::uint32_t buffers, std::uint32_t images)
{
    buffer_types_ = buffers;
    image_types_ = images;
}

void ScriptedVulkan::miscompute() { miscompute_ = true; }
void ScriptedVulkan::misdraw() { misdraw_ = true; }
void ScriptedVulkan::hold_fences() { hold_fences_ = true; }

void ScriptedVulkan::answer_fault(VkResult fault, VkResult idle_after)
{
    fault_answer_ = fault;
    idle_after_fault_ = idle_after;
}

void ScriptedVulkan::change_after_fault(std::uint64_t luid_xor, VkQueueFlags flags)
{
    luid_xor_ = luid_xor;
    flags_after_fault_ = flags;
}

std::uint32_t ScriptedVulkan::live() const
{
    std::uint32_t count = 0u;
    for (const Object &object : objects_) count += object.kind != Kind::none ? 1u : 0u;
    return count;
}

std::uint32_t ScriptedVulkan::misuses() const { return misuses_; }
std::uint32_t ScriptedVulkan::instance_api_version() const { return api_version_; }
std::uint32_t ScriptedVulkan::allocated_type(std::uint32_t index) const { return allocated_types_[index]; }
std::uint32_t ScriptedVulkan::dispatched_groups() const { return dispatched_groups_; }
std::uint32_t ScriptedVulkan::drawn_vertices() const { return drawn_vertices_; }
std::uint64_t ScriptedVulkan::shader_words() const { return shader_words_; }
std::uint32_t ScriptedVulkan::device_parent() const { return device_parent_; }

VkResult ScriptedVulkan::use(Call call)
{
    const auto index = static_cast<std::uint32_t>(call);
    calls_[index]++;
    return calls_[index] == fail_at_[index] ? fail_with_[index] : VK_SUCCESS;
}

std::uint64_t ScriptedVulkan::make(Kind kind, std::uint64_t link)
{
    for (Object &object : objects_) {
        if (object.kind != Kind::none) continue;
        object = Object{next_handle_++, kind, link};
        return object.handle;
    }
    misuses_++;
    return 0u;
}

ScriptedVulkan::Object *ScriptedVulkan::find(Kind kind, std::uint64_t handle)
{
    for (Object &object : objects_)
        if (object.kind == kind && object.handle == handle) return &object;
    return nullptr;
}

/* Destroying VK_NULL_HANDLE is the no-op Vulkan makes it. */
void ScriptedVulkan::destroy(Kind kind, std::uint64_t handle)
{
    if (handle == 0u) return;
    Object *const object = find(kind, handle);
    if (object == nullptr) {
        misuses_++;
        return;
    }
    *object = Object{};
}

ScriptedVulkan::Allocation *ScriptedVulkan::allocation_of(std::uint64_t memory)
{
    for (Allocation &allocation : allocations_)
        if (memory != 0u && allocation.memory == memory) return &allocation;
    return nullptr;
}

VkResult ScriptedVulkan::create_instance(const VkInstanceCreateInfo *info, VkInstance *instance)
{
    const VkResult result = use(Call::create_instance);
    if (result != VK_SUCCESS) return result;
    api_version_ = info->pApplicationInfo != nullptr ? info->pApplicationInfo->apiVersion : 0u;
    instance_name_count_ = 0u;
    for (std::uint32_t index = 0u; index < info->enabledExtensionCount && index < max_enabled; index++)
        instance_names_[instance_name_count_++] = info->ppEnabledExtensionNames[index];
    lost_ = false;
    *instance = as_handle<VkInstance>(make(Kind::instance, 0u));
    return VK_SUCCESS;
}

void ScriptedVulkan::destroy_instance(VkInstance instance) { destroy(Kind::instance, raw(instance)); }

VkResult ScriptedVulkan::enumerate_physical_devices(VkInstance, std::uint32_t *count,
                                                    VkPhysicalDevice *physical_devices)
{
    const VkResult result = use(Call::enumerate_physical_devices);
    if (result != VK_SUCCESS) return result;
    if (physical_devices == nullptr) {
        *count = physical_devices_;
        return VK_SUCCESS;
    }
    const std::uint32_t written = *count < physical_devices_ ? *count : physical_devices_;
    for (std::uint32_t index = 0u; index < written; index++)
        physical_devices[index] = as_handle<VkPhysicalDevice>(physical_device_base + index);
    *count = written;
    return written < physical_devices_ ? VK_INCOMPLETE : VK_SUCCESS;
}

void ScriptedVulkan::physical_device_properties2(VkPhysicalDevice physical_device,
                                                 VkPhysicalDeviceProperties2 *properties)
{
    const std::uint64_t index = raw(physical_device) - physical_device_base;
    properties->properties.apiVersion = (1u << 22u) | (3u << 12u);
    for (auto *node = static_cast<VkBaseOutStructure *>(properties->pNext); node != nullptr; node = node->pNext) {
        if (node->sType != VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES) continue;
        auto *const id = reinterpret_cast<VkPhysicalDeviceIDProperties *>(node);
        std::memcpy(id->deviceLUID, &luids_[index], sizeof luids_[index]);
        id->deviceLUIDValid = luid_valid_[index] ? VK_TRUE : VK_FALSE;
    }
}

void ScriptedVulkan::queue_family_properties(VkPhysicalDevice, std::uint32_t *count,
                                             VkQueueFamilyProperties *properties)
{
    if (properties == nullptr) {
        *count = queue_families_;
        return;
    }
    const std::uint32_t written = *count < queue_families_ ? *count : queue_families_;
    for (std::uint32_t index = 0u; index < written; index++) {
        properties[index] = VkQueueFamilyProperties{};
        properties[index].queueFlags = queue_flags_;
        properties[index].queueCount = 1u;
    }
    *count = written;
}

void ScriptedVulkan::memory_properties(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties *properties)
{
    *properties = memory_;
}

VkResult ScriptedVulkan::create_device(VkPhysicalDevice physical_device, const VkDeviceCreateInfo *info,
                                       VkDevice *device)
{
    const VkResult result = use(Call::create_device);
    if (result != VK_SUCCESS) return result;
    if (info->queueCreateInfoCount != 1u) misuses_++;
    device_name_count_ = 0u;
    for (std::uint32_t index = 0u; index < info->enabledExtensionCount && index < max_enabled; index++)
        device_names_[device_name_count_++] = info->ppEnabledExtensionNames[index];
    device_parent_ = static_cast<std::uint32_t>(raw(physical_device) - physical_device_base);
    *device = as_handle<VkDevice>(make(Kind::device, 0u));
    return VK_SUCCESS;
}

VkResult ScriptedVulkan::force_context_fault(VkInstance, VkPhysicalDevice)
{
    const VkResult result = use(Call::force_context_fault);
    lost_ = true;
    for (std::uint64_t &luid : luids_) luid ^= luid_xor_;
    queue_flags_ = flags_after_fault_;
    return result != VK_SUCCESS ? result : fault_answer_;
}

void ScriptedVulkan::destroy_device(VkDevice device) { destroy(Kind::device, raw(device)); }

VkResult ScriptedVulkan::device_wait_idle(VkDevice)
{
    const VkResult result = use(Call::device_wait_idle);
    if (result != VK_SUCCESS) return result;
    return lost_ ? idle_after_fault_ : VK_SUCCESS;
}

void ScriptedVulkan::device_queue(VkDevice, std::uint32_t family, std::uint32_t index, VkQueue *queue)
{
    *queue = as_handle<VkQueue>(queue_base + family * max_physical_devices + index);
}

VkResult ScriptedVulkan::create_buffer(VkDevice, const VkBufferCreateInfo *info, VkBuffer *buffer)
{
    const VkResult result = use(Call::create_buffer);
    if (result != VK_SUCCESS) return result;
    *buffer = as_handle<VkBuffer>(make(Kind::buffer, 0u));
    static_cast<void>(info);
    return VK_SUCCESS;
}

void ScriptedVulkan::destroy_buffer(VkDevice, VkBuffer buffer) { destroy(Kind::buffer, raw(buffer)); }

void ScriptedVulkan::buffer_memory_requirements(VkDevice, VkBuffer, VkMemoryRequirements *requirements)
{
    requirements->size = max_mapped_bytes;
    requirements->alignment = 4096u;
    requirements->memoryTypeBits = buffer_types_;
}

VkResult ScriptedVulkan::bind_buffer_memory(VkDevice, VkBuffer buffer, VkDeviceMemory memory)
{
    const VkResult result = use(Call::bind_buffer_memory);
    if (result != VK_SUCCESS) return result;
    Object *const object = find(Kind::buffer, raw(buffer));
    if (object == nullptr || allocation_of(raw(memory)) == nullptr) {
        misuses_++;
        return VK_ERROR_UNKNOWN;
    }
    object->link = raw(memory);
    return VK_SUCCESS;
}

VkResult ScriptedVulkan::create_image(VkDevice, const VkImageCreateInfo *info, VkImage *image)
{
    const VkResult result = use(Call::create_image);
    if (result != VK_SUCCESS) return result;
    if (info->format != VK_FORMAT_R8G8B8A8_UNORM) misuses_++;
    *image = as_handle<VkImage>(make(Kind::image, 0u));
    return VK_SUCCESS;
}

void ScriptedVulkan::destroy_image(VkDevice, VkImage image) { destroy(Kind::image, raw(image)); }

void ScriptedVulkan::image_memory_requirements(VkDevice, VkImage, VkMemoryRequirements *requirements)
{
    requirements->size = max_mapped_bytes;
    requirements->alignment = 4096u;
    requirements->memoryTypeBits = image_types_;
}

VkResult ScriptedVulkan::bind_image_memory(VkDevice, VkImage image, VkDeviceMemory memory)
{
    const VkResult result = use(Call::bind_image_memory);
    if (result != VK_SUCCESS) return result;
    if (find(Kind::image, raw(image)) == nullptr || allocation_of(raw(memory)) == nullptr) misuses_++;
    return VK_SUCCESS;
}

VkResult ScriptedVulkan::allocate_memory(VkDevice, const VkMemoryAllocateInfo *info, VkDeviceMemory *memory)
{
    const VkResult result = use(Call::allocate_memory);
    if (result != VK_SUCCESS) return result;
    if (allocate_nothing_) {
        *memory = VK_NULL_HANDLE;
        return VK_SUCCESS;
    }
    for (Allocation &allocation : allocations_) {
        if (allocation.memory != 0u) continue;
        allocation.memory = make(Kind::memory, 0u);
        allocation.size = info->allocationSize;
        allocation.type = info->memoryTypeIndex;
        std::memset(allocation.bytes, 0, sizeof allocation.bytes);
        allocated_types_[allocations_made_++] = info->memoryTypeIndex;
        *memory = as_handle<VkDeviceMemory>(allocation.memory);
        return VK_SUCCESS;
    }
    return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

void ScriptedVulkan::report_external_objects(void *handle)
{
    external_objects_ = true;
    win32_handle_ = handle;
}

void ScriptedVulkan::answer_extensions(std::uint32_t count, VkResult first, VkResult second)
{
    scripted_extensions_ = true;
    extension_count_ = count;
    extension_first_ = first;
    extension_second_ = second;
}

void ScriptedVulkan::allocate_nothing() { allocate_nothing_ = true; }

void ScriptedVulkan::hand_out_no_handle() { no_handle_ = true; }

void ScriptedVulkan::refuse_the_release(std::uint32_t code) { release_code_ = code; }

VkResult ScriptedVulkan::enumerate_device_extensions(VkPhysicalDevice, std::uint32_t *count,
                                                     VkExtensionProperties *properties)
{
    static const char *const reported[] = {"VK_KHR_external_memory_win32", "VK_KHR_external_semaphore_win32"};
    static const char *const others[] = {"VK_KHR_external_memory_fd", "VK_KHR_swapchain"};
    const std::uint32_t      listed = scripted_extensions_ ? extension_count_ : (external_objects_ ? 2u : 0u);
    if (properties == nullptr) {
        *count = listed;
        return scripted_extensions_ ? extension_first_ : VK_SUCCESS;
    }
    const std::uint32_t room = *count < listed ? *count : listed;
    for (std::uint32_t index = 0u; index < room; index++) {
        properties[index] = VkExtensionProperties{};
        const char *const name = scripted_extensions_ ? others[index % 2u] : reported[index];
        for (std::uint32_t at = 0u; name[at] != '\0' && at + 1u < sizeof properties[index].extensionName; at++)
            properties[index].extensionName[at] = name[at];
        properties[index].specVersion = 1u;
    }
    *count = room;
    if (scripted_extensions_) return extension_second_;
    return room < listed ? VK_INCOMPLETE : VK_SUCCESS;
}

std::uint32_t ScriptedVulkan::last_kernel_status(VkInstance, VkPhysicalDevice)
{
    return kernel_status_;
}

void ScriptedVulkan::set_kernel_status(std::uint32_t code)
{
    kernel_status_ = code;
}

VkResult ScriptedVulkan::memory_win32_handle(VkDevice, VkDeviceMemory memory, void **handle)
{
    *handle = nullptr;
    if (!external_objects_) return VK_ERROR_EXTENSION_NOT_PRESENT;
    if (memory == VK_NULL_HANDLE) return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    if (no_handle_) return VK_SUCCESS;
    *handle = win32_handle_;
    return win32_handle_ != nullptr ? VK_SUCCESS : VK_ERROR_INVALID_EXTERNAL_HANDLE;
}

void ScriptedVulkan::free_memory(VkDevice, VkDeviceMemory memory)
{
    Allocation *const allocation = allocation_of(raw(memory));
    if (allocation != nullptr) allocation->memory = 0u;
    destroy(Kind::memory, raw(memory));
    if (release_code_ != 0u) kernel_status_ = release_code_;
}

VkResult ScriptedVulkan::map_memory(VkDevice, VkDeviceMemory memory, void **data)
{
    const VkResult result = use(Call::map_memory);
    if (result != VK_SUCCESS) return result;
    Allocation *const allocation = allocation_of(raw(memory));
    const bool visible = allocation != nullptr &&
                         (memory_.memoryTypes[allocation->type].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0u;
    if (!visible) {
        misuses_++;
        return VK_ERROR_MEMORY_MAP_FAILED;
    }
    *data = allocation->bytes;
    return VK_SUCCESS;
}

VkResult ScriptedVulkan::create_image_view(VkDevice, const VkImageViewCreateInfo *, VkImageView *view)
{
    const VkResult result = use(Call::create_image_view);
    if (result == VK_SUCCESS) *view = as_handle<VkImageView>(make(Kind::view, 0u));
    return result;
}

void ScriptedVulkan::destroy_image_view(VkDevice, VkImageView view) { destroy(Kind::view, raw(view)); }

VkResult ScriptedVulkan::create_shader_module(VkDevice, const VkShaderModuleCreateInfo *info, VkShaderModule *module)
{
    const VkResult result = use(Call::create_shader_module);
    if (result != VK_SUCCESS) return result;
    shader_words_ += info->codeSize / sizeof(std::uint32_t);
    *module = as_handle<VkShaderModule>(make(Kind::shader, 0u));
    return VK_SUCCESS;
}

void ScriptedVulkan::destroy_shader_module(VkDevice, VkShaderModule module) { destroy(Kind::shader, raw(module)); }

VkResult ScriptedVulkan::create_descriptor_set_layout(VkDevice, const VkDescriptorSetLayoutCreateInfo *,
                                                      VkDescriptorSetLayout *layout)
{
    const VkResult result = use(Call::create_descriptor_set_layout);
    if (result == VK_SUCCESS) *layout = as_handle<VkDescriptorSetLayout>(make(Kind::set_layout, 0u));
    return result;
}

void ScriptedVulkan::destroy_descriptor_set_layout(VkDevice, VkDescriptorSetLayout layout)
{
    destroy(Kind::set_layout, raw(layout));
}

VkResult ScriptedVulkan::create_pipeline_layout(VkDevice, const VkPipelineLayoutCreateInfo *,
                                                VkPipelineLayout *layout)
{
    const VkResult result = use(Call::create_pipeline_layout);
    if (result == VK_SUCCESS) *layout = as_handle<VkPipelineLayout>(make(Kind::pipeline_layout, 0u));
    return result;
}

void ScriptedVulkan::destroy_pipeline_layout(VkDevice, VkPipelineLayout layout)
{
    destroy(Kind::pipeline_layout, raw(layout));
}

VkResult ScriptedVulkan::create_compute_pipeline(VkDevice, const VkComputePipelineCreateInfo *,
                                                 VkPipeline *pipeline)
{
    const VkResult result = use(Call::create_compute_pipeline);
    if (result == VK_SUCCESS) *pipeline = as_handle<VkPipeline>(make(Kind::pipeline, 0u));
    return result;
}

VkResult ScriptedVulkan::create_graphics_pipeline(VkDevice, const VkGraphicsPipelineCreateInfo *,
                                                  VkPipeline *pipeline)
{
    const VkResult result = use(Call::create_graphics_pipeline);
    if (result == VK_SUCCESS) *pipeline = as_handle<VkPipeline>(make(Kind::pipeline, 0u));
    return result;
}

void ScriptedVulkan::destroy_pipeline(VkDevice, VkPipeline pipeline) { destroy(Kind::pipeline, raw(pipeline)); }

VkResult ScriptedVulkan::create_descriptor_pool(VkDevice, const VkDescriptorPoolCreateInfo *, VkDescriptorPool *pool)
{
    const VkResult result = use(Call::create_descriptor_pool);
    if (result == VK_SUCCESS) *pool = as_handle<VkDescriptorPool>(make(Kind::descriptor_pool, 0u));
    return result;
}

void ScriptedVulkan::destroy_descriptor_pool(VkDevice, VkDescriptorPool pool)
{
    destroy(Kind::descriptor_pool, raw(pool));
}

/* A set is not an object of its own: it goes with its pool. */
VkResult ScriptedVulkan::allocate_descriptor_set(VkDevice, const VkDescriptorSetAllocateInfo *info,
                                                 VkDescriptorSet *set)
{
    const VkResult result = use(Call::allocate_descriptor_set);
    if (result != VK_SUCCESS) return result;
    if (find(Kind::descriptor_pool, raw(info->descriptorPool)) == nullptr) misuses_++;
    *set = as_handle<VkDescriptorSet>(next_handle_++);
    return VK_SUCCESS;
}

void ScriptedVulkan::update_descriptor_set(VkDevice, const VkWriteDescriptorSet *write)
{
    recording_.set = raw(write->pBufferInfo[0].buffer);
}

VkResult ScriptedVulkan::create_render_pass(VkDevice, const VkRenderPassCreateInfo *, VkRenderPass *render_pass)
{
    const VkResult result = use(Call::create_render_pass);
    if (result == VK_SUCCESS) *render_pass = as_handle<VkRenderPass>(make(Kind::render_pass, 0u));
    return result;
}

void ScriptedVulkan::destroy_render_pass(VkDevice, VkRenderPass render_pass)
{
    destroy(Kind::render_pass, raw(render_pass));
}

VkResult ScriptedVulkan::create_framebuffer(VkDevice, const VkFramebufferCreateInfo *, VkFramebuffer *framebuffer)
{
    const VkResult result = use(Call::create_framebuffer);
    if (result == VK_SUCCESS) *framebuffer = as_handle<VkFramebuffer>(make(Kind::framebuffer, 0u));
    return result;
}

void ScriptedVulkan::destroy_framebuffer(VkDevice, VkFramebuffer framebuffer)
{
    destroy(Kind::framebuffer, raw(framebuffer));
}

VkResult ScriptedVulkan::create_command_pool(VkDevice, const VkCommandPoolCreateInfo *, VkCommandPool *pool)
{
    const VkResult result = use(Call::create_command_pool);
    if (result == VK_SUCCESS) *pool = as_handle<VkCommandPool>(make(Kind::command_pool, 0u));
    return result;
}

void ScriptedVulkan::destroy_command_pool(VkDevice, VkCommandPool pool) { destroy(Kind::command_pool, raw(pool)); }

/* A command buffer goes with its pool. */
VkResult ScriptedVulkan::allocate_command_buffer(VkDevice, const VkCommandBufferAllocateInfo *info,
                                                 VkCommandBuffer *command_buffer)
{
    const VkResult result = use(Call::allocate_command_buffer);
    if (result != VK_SUCCESS) return result;
    if (find(Kind::command_pool, raw(info->commandPool)) == nullptr) misuses_++;
    *command_buffer = as_handle<VkCommandBuffer>(command_buffer_base);
    return VK_SUCCESS;
}

VkResult ScriptedVulkan::create_fence(VkDevice, const VkFenceCreateInfo *, VkFence *fence)
{
    const VkResult result = use(Call::create_fence);
    if (result == VK_SUCCESS) *fence = as_handle<VkFence>(make(Kind::fence, 0u));
    return result;
}

void ScriptedVulkan::destroy_fence(VkDevice, VkFence fence) { destroy(Kind::fence, raw(fence)); }

VkResult ScriptedVulkan::wait_for_fence(VkDevice, VkFence fence, std::uint64_t)
{
    const VkResult result = use(Call::wait_for_fence);
    if (result != VK_SUCCESS) return result;
    const Object *const object = find(Kind::fence, raw(fence));
    return object != nullptr && object->link != 0u ? VK_SUCCESS : VK_TIMEOUT;
}

/* The set the last descriptor update named survives a begin: the probe
 * updates before it records. */
VkResult ScriptedVulkan::begin_command_buffer(VkCommandBuffer, const VkCommandBufferBeginInfo *)
{
    const VkResult result = use(Call::begin_command_buffer);
    const std::uint64_t set = recording_.set;
    recording_ = Recording{};
    recording_.set = set;
    return result;
}

VkResult ScriptedVulkan::end_command_buffer(VkCommandBuffer) { return use(Call::end_command_buffer); }

void ScriptedVulkan::cmd_bind_pipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline pipeline)
{
    if (find(Kind::pipeline, raw(pipeline)) == nullptr) misuses_++;
}

void ScriptedVulkan::cmd_bind_compute_set(VkCommandBuffer, VkPipelineLayout layout, VkDescriptorSet)
{
    if (find(Kind::pipeline_layout, raw(layout)) == nullptr) misuses_++;
}

void ScriptedVulkan::cmd_dispatch(VkCommandBuffer, std::uint32_t groups_x)
{
    recording_.groups = groups_x;
    dispatched_groups_ = groups_x;
}

void ScriptedVulkan::cmd_begin_render_pass(VkCommandBuffer, const VkRenderPassBeginInfo *info)
{
    if (find(Kind::framebuffer, raw(info->framebuffer)) == nullptr) misuses_++;
    recording_.in_render_pass = true;
}

void ScriptedVulkan::cmd_end_render_pass(VkCommandBuffer) { recording_.in_render_pass = false; }

void ScriptedVulkan::cmd_draw(VkCommandBuffer, std::uint32_t vertices)
{
    recording_.drew = recording_.in_render_pass;
    recording_.vertices = vertices;
    drawn_vertices_ = vertices;
}

void ScriptedVulkan::cmd_copy_image_to_buffer(VkCommandBuffer, VkImage image, VkImageLayout, VkBuffer buffer,
                                              const VkBufferImageCopy *)
{
    if (find(Kind::image, raw(image)) == nullptr) misuses_++;
    recording_.copied_to = raw(buffer);
}

void ScriptedVulkan::cmd_fill_buffer(VkCommandBuffer, VkBuffer buffer, VkDeviceSize size, uint32_t value)
{
    if (size == 0u) misuses_++;
    recording_.filled = raw(buffer);
    recording_.fill_value = value;
}

VkResult ScriptedVulkan::queue_submit(VkQueue, const VkSubmitInfo *submit, VkFence fence)
{
    const VkResult result = use(Call::queue_submit);
    if (result != VK_SUCCESS) return result;
    if (submit->commandBufferCount != 1u) misuses_++;
    if (hold_fences_) return VK_SUCCESS;
    run(recording_);
    Object *const signalled = find(Kind::fence, raw(fence));
    if (signalled != nullptr) signalled->link = 1u;
    return VK_SUCCESS;
}

void ScriptedVulkan::run(const Recording &recording)
{
    if (recording.groups != 0u) {
        const Object *const buffer = find(Kind::buffer, recording.set);
        Allocation *const target = buffer != nullptr ? allocation_of(buffer->link) : nullptr;
        if (target == nullptr) return;
        const std::uint32_t elements = recording.groups * local_size_x - (miscompute_ ? 1u : 0u);
        for (std::uint32_t index = 0u; index < elements; index++) {
            std::uint32_t value = 0u;
            std::memcpy(&value, target->bytes + index * element_bytes, sizeof value);
            value *= 2u;
            std::memcpy(target->bytes + index * element_bytes, &value, sizeof value);
        }
    }
    if (recording.filled != 0u) {
        const Object *const buffer = find(Kind::buffer, recording.filled);
        Allocation *const   target = buffer != nullptr ? allocation_of(buffer->link) : nullptr;
        if (target == nullptr) return;
        for (std::uint64_t at = 0u; at + sizeof recording.fill_value <= max_mapped_bytes;
             at += sizeof recording.fill_value)
            std::memcpy(target->bytes + at, &recording.fill_value, sizeof recording.fill_value);
    }
    if (recording.drew) drew_since_present_ = true;
    if (recording.drew && recording.copied_to != 0u) {
        const Object *const buffer = find(Kind::buffer, recording.copied_to);
        Allocation *const target = buffer != nullptr ? allocation_of(buffer->link) : nullptr;
        if (target == nullptr) return;
        for (std::uint64_t at = 0u; at + pixel_bytes <= max_mapped_bytes; at += pixel_bytes)
            std::memcpy(target->bytes + at, colour, sizeof colour);
        if (misdraw_) std::memset(target->bytes + max_mapped_bytes - pixel_bytes, 0, pixel_bytes);
    }
}

void ScriptedVulkan::present_into(ScriptedWindowFrame &window) { window_ = &window; }

void ScriptedVulkan::present_unpaced() { pacing_kmt_ = nullptr; }

void ScriptedVulkan::pace_on(Kmt &kmt, std::uint32_t adapter, std::uint32_t device)
{
    pacing_kmt_ = &kmt;
    pacing_adapter_ = adapter;
    pacing_device_ = device;
}

void ScriptedVulkan::hold_the_surface_extent(std::uint32_t width, std::uint32_t height)
{
    held_extent_ = true;
    held_width_ = width;
    held_height_ = height;
}

void ScriptedVulkan::offer_nothing_presentable() { nothing_presentable_ = true; }
void ScriptedVulkan::lose_the_surface_queries() { lost_surface_ = true; }
void ScriptedVulkan::lose_the_present_modes() { lost_present_modes_ = true; }
void ScriptedVulkan::ask_for_images(std::uint32_t minimum) { least_images_ = minimum; }
void ScriptedVulkan::lose_the_swapchain_images() { lost_swapchain_images_ = true; }
void ScriptedVulkan::hand_out_one_image() { one_image_ = true; }

void ScriptedVulkan::offer_only_the_mode(VkPresentModeKHR mode)
{
    one_mode_ = true;
    only_mode_ = mode;
}

bool ScriptedVulkan::instance_enabled(const char *name) const
{
    for (std::uint32_t index = 0u; index < instance_name_count_; index++)
        if (std::strcmp(instance_names_[index], name) == 0) return true;
    return false;
}

bool ScriptedVulkan::device_enabled(const char *name) const
{
    for (std::uint32_t index = 0u; index < device_name_count_; index++)
        if (std::strcmp(device_names_[index], name) == 0) return true;
    return false;
}

std::uint32_t ScriptedVulkan::presents() const { return presents_; }

VkResult ScriptedVulkan::create_win32_surface(VkInstance instance, void *, void *window, VkSurfaceKHR *surface)
{
    const VkResult result = use(Call::create_win32_surface);
    if (result != VK_SUCCESS) return result;
    if (find(Kind::instance, raw(instance)) == nullptr) misuses_++;
    window_handle_ = window;
    *surface = as_handle<VkSurfaceKHR>(make(Kind::surface, 0u));
    return VK_SUCCESS;
}

void ScriptedVulkan::destroy_surface(VkInstance, VkSurfaceKHR surface) { destroy(Kind::surface, raw(surface)); }

/* The window's own size, unless a test holds the surface at another: a
 * swapchain of the wrong extent is what an application meets mid-resize. */
VkResult ScriptedVulkan::surface_capabilities(VkPhysicalDevice, VkSurfaceKHR surface,
                                              VkSurfaceCapabilitiesKHR *capabilities)
{
    if (find(Kind::surface, raw(surface)) == nullptr) return VK_ERROR_SURFACE_LOST_KHR;
    std::uint32_t width = 0u;
    std::uint32_t height = 0u;
    if (window_ == nullptr || !window_->size(window_handle_, width, height)) return VK_ERROR_SURFACE_LOST_KHR;
    if (held_extent_) {
        width = held_width_;
        height = held_height_;
    }
    *capabilities = VkSurfaceCapabilitiesKHR{};
    capabilities->minImageCount = least_images_;
    capabilities->maxImageCount = images_per_swapchain * 4u;
    capabilities->currentExtent = {width, height};
    capabilities->maxImageExtent = {width, height};
    capabilities->maxImageArrayLayers = 1u;
    capabilities->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    capabilities->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    capabilities->supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    capabilities->supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    return VK_SUCCESS;
}

VkResult ScriptedVulkan::surface_formats(VkPhysicalDevice, VkSurfaceKHR surface, std::uint32_t *count,
                                         VkSurfaceFormatKHR *formats)
{
    if (find(Kind::surface, raw(surface)) == nullptr || lost_surface_) return VK_ERROR_SURFACE_LOST_KHR;
    const VkSurfaceFormatKHR presentable[] = {{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
                                              {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    const VkSurfaceFormatKHR unpresentable[] = {{VK_FORMAT_R5G6B5_UNORM_PACK16, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
                                                {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT}};
    const VkSurfaceFormatKHR *const offered = nothing_presentable_ ? unpresentable : presentable;
    const std::uint32_t available = 2u;
    if (formats == nullptr) {
        *count = available;
        return VK_SUCCESS;
    }
    const std::uint32_t written = *count < available ? *count : available;
    for (std::uint32_t index = 0u; index < written; index++) formats[index] = offered[index];
    *count = written;
    return written < available ? VK_INCOMPLETE : VK_SUCCESS;
}

VkResult ScriptedVulkan::surface_present_modes(VkPhysicalDevice, VkSurfaceKHR surface, std::uint32_t *count,
                                               VkPresentModeKHR *modes)
{
    if (find(Kind::surface, raw(surface)) == nullptr || lost_surface_ || lost_present_modes_)
        return VK_ERROR_SURFACE_LOST_KHR;
    const VkPresentModeKHR presentable[] = {VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR};
    const VkPresentModeKHR unpresentable[] = {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_RELAXED_KHR};
    const VkPresentModeKHR only[] = {only_mode_, only_mode_};
    const VkPresentModeKHR *const offered = one_mode_ ? only : (nothing_presentable_ ? unpresentable : presentable);
    const std::uint32_t           available = 2u;
    if (modes == nullptr) {
        *count = available;
        return VK_SUCCESS;
    }
    const std::uint32_t written = *count < available ? *count : available;
    for (std::uint32_t index = 0u; index < written; index++) modes[index] = offered[index];
    *count = written;
    return written < available ? VK_INCOMPLETE : VK_SUCCESS;
}

VkResult ScriptedVulkan::create_swapchain(VkDevice device, const VkSwapchainCreateInfoKHR *info,
                                          VkSwapchainKHR *swapchain)
{
    const VkResult result = use(Call::create_swapchain);
    if (result != VK_SUCCESS) return result;
    if (find(Kind::device, raw(device)) == nullptr) misuses_++;
    if (find(Kind::surface, raw(info->surface)) == nullptr) return VK_ERROR_SURFACE_LOST_KHR;
    /* One swapchain is retired by the next, as the specification has it: the
     * old one is still there until the application destroys it. */
    if (swapchain_ != 0u) {
        if (retired_ != 0u || raw(info->oldSwapchain) != swapchain_) return VK_ERROR_OUT_OF_HOST_MEMORY;
        retired_ = swapchain_;
        for (std::uint32_t index = 0u; index < images_per_swapchain; index++)
            retired_images_[index] = swapchain_images_[index];
        swapchain_ = 0u;
    }
    swapchain_width_ = info->imageExtent.width;
    swapchain_height_ = info->imageExtent.height;
    for (std::uint32_t index = 0u; index < images_per_swapchain; index++)
        swapchain_images_[index] = make(Kind::image, 0u);
    next_image_ = 0u;
    swapchain_ = make(Kind::swapchain, 0u);
    *swapchain = as_handle<VkSwapchainKHR>(swapchain_);
    return VK_SUCCESS;
}

void ScriptedVulkan::destroy_swapchain(VkDevice, VkSwapchainKHR swapchain)
{
    const bool retired = raw(swapchain) == retired_ && retired_ != 0u;
    if (raw(swapchain) != swapchain_ && !retired) {
        misuses_++;
        return;
    }
    std::uint64_t *const kept = retired ? retired_images_ : swapchain_images_;
    for (std::uint32_t index = 0u; index < images_per_swapchain; index++) {
        destroy(Kind::image, kept[index]);
        kept[index] = 0u;
    }
    destroy(Kind::swapchain, retired ? retired_ : swapchain_);
    (retired ? retired_ : swapchain_) = 0u;
}

VkResult ScriptedVulkan::swapchain_images(VkDevice, VkSwapchainKHR swapchain, std::uint32_t *count, VkImage *images)
{
    if (raw(swapchain) != swapchain_ || swapchain_ == 0u) return VK_ERROR_OUT_OF_DATE_KHR;
    if (lost_swapchain_images_) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    const std::uint32_t available = one_image_ ? 1u : images_per_swapchain;
    if (images == nullptr) {
        *count = available;
        return VK_SUCCESS;
    }
    const std::uint32_t written = *count < available ? *count : available;
    for (std::uint32_t index = 0u; index < written; index++) images[index] = as_handle<VkImage>(swapchain_images_[index]);
    *count = written;
    return written < available ? VK_INCOMPLETE : VK_SUCCESS;
}

/* The window is what decides: one of another size than the swapchain has left
 * it out of date, as the driver's own acquire does. */
VkResult ScriptedVulkan::acquire_next_image(VkDevice, VkSwapchainKHR swapchain, std::uint64_t, std::uint32_t *index)
{
    const VkResult result = use(Call::acquire_next_image);
    if (result != VK_SUCCESS) return result;
    if (raw(swapchain) != swapchain_ || swapchain_ == 0u) return VK_ERROR_OUT_OF_DATE_KHR;
    std::uint32_t width = 0u;
    std::uint32_t height = 0u;
    if (window_ == nullptr || !window_->size(window_handle_, width, height)) return VK_ERROR_SURFACE_LOST_KHR;
    if (width != swapchain_width_ || height != swapchain_height_) return VK_ERROR_OUT_OF_DATE_KHR;
    *index = next_image_;
    next_image_ = (next_image_ + 1u) % images_per_swapchain;
    return VK_SUCCESS;
}

/* What the driver's present does, as far as the probe can see it: the drawn
 * frame on the window, in the window's channel order. */
VkResult ScriptedVulkan::queue_present(VkQueue, const VkPresentInfoKHR *info)
{
    const VkResult result = use(Call::queue_present);
    if (result != VK_SUCCESS) return result;
    if (info->swapchainCount != 1u || raw(info->pSwapchains[0]) != swapchain_) return VK_ERROR_OUT_OF_DATE_KHR;
    std::uint32_t width = 0u;
    std::uint32_t height = 0u;
    if (window_ == nullptr || !window_->size(window_handle_, width, height)) return VK_ERROR_SURFACE_LOST_KHR;
    if (width != swapchain_width_ || height != swapchain_height_) return VK_ERROR_OUT_OF_DATE_KHR;
    if (!drew_since_present_) return VK_SUCCESS;
    drew_since_present_ = false;
    presents_++;
    if (pacing_kmt_ != nullptr)
        static_cast<void>(pacing_kmt_->wait_for_vertical_blank(pacing_adapter_, pacing_device_, 0u));
    const std::uint8_t window_order[4] = {colour[2], colour[1], colour[0], colour[3]};
    if (misdraw_)
        window_->show_but_the_last(window_order);
    else
        window_->show(window_order);
    return VK_SUCCESS;
}

}  // namespace av
