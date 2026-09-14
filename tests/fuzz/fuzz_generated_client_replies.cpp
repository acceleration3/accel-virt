/* Hostile replies into the generated Vulkan and OpenCL clients (feature 044,
 * T036).
 *
 * A client reads back bytes the host wrote, into memory the application owns:
 * a count, a length or a byte total in a reply is the host's word, and the
 * application's array holds only what the application said it does. So what
 * must hold, whatever the reply: nothing is written outside the caller's
 * outputs, a count the caller reads back never claims more of its array than
 * the room it gave, an output structure's array is still the caller's, and an
 * info query that succeeded never reports a size past the buffer it filled.
 * ASan and UBSan judge the rest.
 *
 * An input is one call: a byte choosing it, a byte choosing the room the
 * caller gives (0 to 4 elements), a byte of flags - bit 0, the caller gives its
 * array or buffer; bit 1, it asks for the count or size, or chains the
 * structures a properties query fills - and then the reply, exactly as the
 * transport hands it over. The calls are the shapes T032 bounded: counted
 * handle, structure and scalar arrays, info values, byte outputs, output
 * structures carrying the caller's array (T037), and plain creates. */
#include "harness/framework/guarded_buffer.hpp"
#include "harness/transport/mock_transport.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"
#include "proto/generated/vulkan/vk_dispatch_client.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::size_t   header_bytes = 3u;
constexpr std::uint32_t room_choices = 5u;
constexpr std::uint8_t  array_given = 1u;
constexpr std::uint8_t  count_asked = 2u;
constexpr std::uint8_t  untouched = 0xC3u;
constexpr std::uint64_t some_handle = 0x1100u;
constexpr std::size_t   info_bytes_per_room = 8u;
constexpr std::size_t   read_bytes_per_room = 4u;

template <typename H>
H handle()
{
    return reinterpret_cast<H>(some_handle);
}

void require(bool holds)
{
    if (!holds) __builtin_trap();
}

/* A name an application may hand to strlen: a zero inside its array. */
bool terminated(const char *name, std::size_t size)
{
    for (std::size_t index = 0u; index < size; index++) {
        if (name[index] == '\0') return true;
    }
    return false;
}

/* `count` elements of T a caller owns, between poisoned guard bands. The bytes
 * hold a pattern no struct is zero in, unless the caller zeroed them, which is
 * what an application does with a structure it reads a count out of. */
template <typename T>
class Output {
public:
    explicit Output(std::size_t count, std::uint8_t pattern = untouched) : buffer_(count * sizeof(T))
    {
        std::memset(buffer_.data(), pattern, buffer_.size());
    }

    T   *get() { return reinterpret_cast<T *>(buffer_.data()); }
    bool intact() const { return buffer_.guards_intact(); }

private:
    av::GuardedBuffer buffer_;
};

class HostileReply {
public:
    HostileReply(const std::uint8_t *data, std::size_t size)
        : room_(data[1] % room_choices), flags_(data[2])
    {
        transport_.set_reply(data + header_bytes, size - header_bytes);
    }

    void run(std::uint8_t selector)
    {
        switch (selector % call_count) {
        case 0: vk_physical_devices(); break;
        case 1: vk_queue_families(); break;
        case 2: vk_queue_families2(); break;
        case 3: vk_device_extensions(); break;
        case 4: vk_device_groups(); break;
        case 5: vk_create_instance(); break;
        case 6: vk_create_buffer(); break;
        case 7: vk_memory_requirements(); break;
        case 8: vk_memory_properties(); break;
        case 9: vk_pipeline_binaries(); break;
        case 10: vk_host_image_copy(); break;
        case 11: vk_format_modifiers(); break;
        case 12: cl_platforms(); break;
        case 13: cl_devices(); break;
        case 14: cl_info(CL_DEVICE_NAME); break;
        case 15: cl_info(CL_PLATFORM_VENDOR); break;
        case 16: cl_info(CL_PROGRAM_BUILD_LOG); break;
        case 17: cl_read(); break;
        case 18: cl_create_context(); break;
        case 19: cl_create_buffer(); break;
        case 20: cl_image_formats(); break;
        default: cl_kernels(); break;
        }
    }

private:
    static constexpr std::uint8_t call_count = 22u;

    bool given() const { return (flags_ & array_given) != 0u; }
    bool asked() const { return (flags_ & count_asked) != 0u; }
    template <typename T>
    T *given_or_null(Output<T> &output) { return given() ? output.get() : nullptr; }

    /* A counted array's count, read back: never past the room the caller gave
     * when it gave an array, whatever the call answered. */
    template <typename T>
    void judge_counted(Output<T> &array, std::uint32_t count)
    {
        require(array.intact());
        if (given()) require(count <= room_);
    }

    void vk_physical_devices()
    {
        Output<std::uint64_t> devices(room_);
        std::uint32_t         count = room_;
        (void)av::vkproto::rvk_vkEnumeratePhysicalDevices(transport_, handle<VkInstance>(), &count,
                                                          given_or_null(devices));
        judge_counted(devices, count);
    }

    void vk_queue_families()
    {
        Output<VkQueueFamilyProperties> families(room_);
        std::uint32_t                   count = room_;
        av::vkproto::rvk_vkGetPhysicalDeviceQueueFamilyProperties(transport_, handle<VkPhysicalDevice>(), &count,
                                                                  given_or_null(families));
        judge_counted(families, count);
    }

    void vk_queue_families2()
    {
        Output<VkQueueFamilyProperties2> families(room_, 0u);
        for (std::uint32_t index = 0u; index < room_; index++)
            families.get()[index].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
        std::uint32_t count = room_;
        av::vkproto::rvk_vkGetPhysicalDeviceQueueFamilyProperties2(transport_, handle<VkPhysicalDevice>(), &count,
                                                                   given_or_null(families));
        judge_counted(families, count);
        for (std::uint32_t index = 0u; index < room_; index++) require(families.get()[index].pNext == nullptr);
    }

    void vk_device_extensions()
    {
        Output<VkExtensionProperties> extensions(room_);
        std::uint32_t                 count = room_;
        const VkResult answered = av::vkproto::rvk_vkEnumerateDeviceExtensionProperties(
            transport_, handle<VkPhysicalDevice>(), nullptr, &count, given_or_null(extensions));
        judge_counted(extensions, count);
        if (!given() || answered < 0) return;
        for (std::uint32_t index = 0u; index < count; index++)
            require(terminated(extensions.get()[index].extensionName, VK_MAX_EXTENSION_NAME_SIZE));
    }

    void vk_device_groups()
    {
        Output<VkPhysicalDeviceGroupProperties> groups(room_, 0u);
        for (std::uint32_t index = 0u; index < room_; index++)
            groups.get()[index].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES;
        std::uint32_t  count = room_;
        const VkResult answered = av::vkproto::rvk_vkEnumeratePhysicalDeviceGroups(transport_, handle<VkInstance>(),
                                                                                   &count, given_or_null(groups));
        judge_counted(groups, count);
        if (!given() || answered < 0) return;
        for (std::uint32_t index = 0u; index < count; index++)
            require(groups.get()[index].physicalDeviceCount <= VK_MAX_DEVICE_GROUP_SIZE);
    }

    void vk_create_instance()
    {
        VkInstanceCreateInfo create = {};
        create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        Output<std::uint64_t> instance(1u);
        (void)av::vkproto::rvk_vkCreateInstance(transport_, &create, nullptr, instance.get());
        require(instance.intact());
    }

    void vk_create_buffer()
    {
        VkBufferCreateInfo create = {};
        create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        create.size = some_handle;
        Output<VkBuffer> buffer(1u);
        (void)av::vkproto::rvk_vkCreateBuffer(transport_, handle<VkDevice>(), &create, nullptr, buffer.get());
        require(buffer.intact());
    }

    void vk_memory_requirements()
    {
        Output<VkMemoryRequirements> requirements(1u);
        av::vkproto::rvk_vkGetBufferMemoryRequirements(transport_, handle<VkDevice>(), handle<VkBuffer>(),
                                                       requirements.get());
        require(requirements.intact());
    }

    /* Counts beside fixed arrays: the structure's own size is the room. */
    void vk_memory_properties()
    {
        Output<VkPhysicalDeviceMemoryProperties> properties(1u, 0u);
        av::vkproto::rvk_vkGetPhysicalDeviceMemoryProperties(transport_, handle<VkPhysicalDevice>(), properties.get());
        require(properties.intact());
        require(properties.get()->memoryTypeCount <= VK_MAX_MEMORY_TYPES);
        require(properties.get()->memoryHeapCount <= VK_MAX_MEMORY_HEAPS);
    }

    void vk_pipeline_binaries()
    {
        VkPipelineBinaryCreateInfoKHR create = {};
        create.sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_CREATE_INFO_KHR;
        Output<std::uint64_t>                  array(room_);
        Output<VkPipelineBinaryHandlesInfoKHR> binaries(1u, 0u);
        VkPipelineBinaryKHR *const caller_array = reinterpret_cast<VkPipelineBinaryKHR *>(given_or_null(array));
        binaries.get()->sType = VK_STRUCTURE_TYPE_PIPELINE_BINARY_HANDLES_INFO_KHR;
        binaries.get()->pipelineBinaryCount = room_;
        binaries.get()->pPipelineBinaries = caller_array;
        (void)av::vkproto::rvk_vkCreatePipelineBinariesKHR(transport_, handle<VkDevice>(), &create, nullptr,
                                                           binaries.get());
        require(binaries.intact());
        require(binaries.get()->pPipelineBinaries == caller_array);
        judge_counted(array, binaries.get()->pipelineBinaryCount);
    }

    /* Two chained structures with the caller's arrays inside, one of scalars
     * and one of chained structures, or no chain at all for the host to
     * invent one into. */
    void vk_host_image_copy()
    {
        Output<VkImageLayout>                               sources(room_);
        Output<VkImageLayout>                               destinations(room_);
        Output<VkPhysicalDeviceLayeredApiPropertiesKHR>     apis(room_, 0u);
        Output<VkPhysicalDeviceHostImageCopyProperties>     copy(1u, 0u);
        Output<VkPhysicalDeviceLayeredApiPropertiesListKHR> layered(1u, 0u);
        Output<VkPhysicalDeviceProperties2>                 properties(1u, 0u);
        for (std::uint32_t index = 0u; index < room_; index++)
            apis.get()[index].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LAYERED_API_PROPERTIES_KHR;
        copy.get()->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES;
        copy.get()->pNext = layered.get();
        copy.get()->copySrcLayoutCount = room_;
        copy.get()->pCopySrcLayouts = given_or_null(sources);
        copy.get()->copyDstLayoutCount = room_;
        copy.get()->pCopyDstLayouts = given_or_null(destinations);
        layered.get()->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LAYERED_API_PROPERTIES_LIST_KHR;
        layered.get()->layeredApiCount = room_;
        layered.get()->pLayeredApis = given_or_null(apis);
        properties.get()->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties.get()->pNext = asked() ? copy.get() : nullptr;
        av::vkproto::rvk_vkGetPhysicalDeviceProperties2(transport_, handle<VkPhysicalDevice>(), properties.get());
        require(properties.intact() && copy.intact() && layered.intact());
        require(properties.get()->pNext == (asked() ? copy.get() : nullptr));
        require(copy.get()->pNext == layered.get() && layered.get()->pNext == nullptr);
        require(copy.get()->pCopySrcLayouts == given_or_null(sources));
        require(copy.get()->pCopyDstLayouts == given_or_null(destinations));
        require(layered.get()->pLayeredApis == given_or_null(apis));
        judge_counted(sources, copy.get()->copySrcLayoutCount);
        judge_counted(destinations, copy.get()->copyDstLayoutCount);
        judge_counted(apis, layered.get()->layeredApiCount);
        for (std::uint32_t index = 0u; index < room_; index++) require(apis.get()[index].pNext == nullptr);
    }

    void vk_format_modifiers()
    {
        Output<VkDrmFormatModifierPropertiesEXT>     modifiers(room_);
        Output<VkDrmFormatModifierPropertiesListEXT> list(1u, 0u);
        Output<VkFormatProperties2>                  properties(1u, 0u);
        list.get()->sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;
        list.get()->drmFormatModifierCount = room_;
        list.get()->pDrmFormatModifierProperties = given_or_null(modifiers);
        properties.get()->sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
        properties.get()->pNext = asked() ? list.get() : nullptr;
        av::vkproto::rvk_vkGetPhysicalDeviceFormatProperties2(transport_, handle<VkPhysicalDevice>(),
                                                              VK_FORMAT_R8G8B8A8_UNORM, properties.get());
        require(properties.intact() && list.intact());
        require(properties.get()->pNext == (asked() ? list.get() : nullptr));
        require(list.get()->pDrmFormatModifierProperties == given_or_null(modifiers));
        judge_counted(modifiers, list.get()->drmFormatModifierCount);
    }

    /* An OpenCL list's count is how many the host has, which the API lets
     * exceed num_entries: only the array and the count itself must be left
     * whole. */
    void cl_platforms()
    {
        Output<std::uint64_t> platforms(room_);
        Output<cl_uint>       total(1u);
        (void)av::clproto::rvk_clGetPlatformIDs(transport_, room_, given_or_null(platforms),
                                                asked() ? total.get() : nullptr);
        require(platforms.intact() && total.intact());
    }

    void cl_devices()
    {
        Output<std::uint64_t> devices(room_);
        Output<cl_uint>       total(1u);
        (void)av::clproto::rvk_clGetDeviceIDs(transport_, some_handle, CL_DEVICE_TYPE_GPU, room_,
                                              given_or_null(devices), asked() ? total.get() : nullptr);
        require(devices.intact() && total.intact());
    }

    /* A value that fit is one no larger than its buffer: an info query that
     * succeeded into a buffer never reports more bytes than the buffer has. */
    void cl_info(std::uint32_t name)
    {
        const std::size_t room = room_ * info_bytes_per_room;
        Output<char>        value(room);
        Output<std::size_t> reported(1u);
        std::size_t *const  report = asked() ? reported.get() : nullptr;
        cl_int              answered = CL_SUCCESS;
        if (name == CL_PROGRAM_BUILD_LOG)
            answered = av::clproto::rvk_clGetProgramBuildInfo(transport_, some_handle, some_handle, name, room,
                                                              given_or_null(value), report);
        else if (name == CL_PLATFORM_VENDOR)
            answered = av::clproto::rvk_clGetPlatformInfo(transport_, some_handle, name, room, given_or_null(value),
                                                          report);
        else
            answered = av::clproto::rvk_clGetDeviceInfo(transport_, some_handle, name, room, given_or_null(value),
                                                        report);
        require(value.intact() && reported.intact());
        if (answered == CL_SUCCESS && given() && asked()) require(*report <= room);
    }

    void cl_read()
    {
        Output<std::uint8_t>  bytes(room_ * read_bytes_per_room);
        Output<std::uint64_t> event(1u);
        (void)av::clproto::rvk_clEnqueueReadBuffer(transport_, some_handle, some_handle, CL_TRUE, 0u,
                                                   room_ * read_bytes_per_room, given_or_null(bytes), 0u, nullptr,
                                                   asked() ? event.get() : nullptr);
        require(bytes.intact() && event.intact());
    }

    void cl_create_context()
    {
        const std::uint64_t device = some_handle;
        Output<cl_int>      error(1u);
        (void)av::clproto::rvk_clCreateContext(transport_, nullptr, 1u, &device, asked() ? error.get() : nullptr);
        require(error.intact());
    }

    void cl_create_buffer()
    {
        Output<cl_int> error(1u);
        (void)av::clproto::rvk_clCreateBuffer(transport_, some_handle, CL_MEM_READ_WRITE, some_handle, nullptr,
                                              asked() ? error.get() : nullptr);
        require(error.intact());
    }

    void cl_image_formats()
    {
        Output<cl_image_format> formats(room_);
        Output<cl_uint>         total(1u);
        (void)av::clproto::rvk_clGetSupportedImageFormats(transport_, some_handle, CL_MEM_READ_WRITE,
                                                          CL_MEM_OBJECT_IMAGE2D, room_, given_or_null(formats),
                                                          room_ * sizeof(cl_image_format),
                                                          asked() ? total.get() : nullptr);
        require(formats.intact() && total.intact());
    }

    void cl_kernels()
    {
        Output<std::uint64_t> kernels(room_);
        Output<cl_uint>       total(1u);
        (void)av::clproto::rvk_clCreateKernelsInProgram(transport_, some_handle, room_, given_or_null(kernels),
                                                        asked() ? total.get() : nullptr);
        require(kernels.intact() && total.intact());
    }

    av::MockTransport   transport_;
    const std::uint32_t room_;
    const std::uint8_t  flags_;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size < header_bytes) return 0;
    HostileReply call(data, size);
    call.run(data[0]);
    return 0;
}
