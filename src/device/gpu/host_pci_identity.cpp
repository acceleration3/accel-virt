#include "device/gpu/host_pci_identity.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "device/gpu/pci_address.hpp"
#include "device/host/host.hpp"

namespace av {

namespace {

constexpr char   sysfs_devices[] = "/sys/bus/pci/devices/";
constexpr size_t path_bytes      = 64u;
constexpr size_t message_bytes   = 96u;

}  // namespace

HostPciIdentity::HostPciIdentity(Host &host)
    : host_(host), vendor_id_(0u), device_id_(0u), revision_(0u), present_(false)
{
    AV_SCOPE("HostPciIdentity::HostPciIdentity");
}

void HostPciIdentity::read(uint32_t gpu_index)
{
    AV_SCOPE("HostPciIdentity::read");
    host_.require_main_loop("HostPciIdentity::read");
    present_ = false;
    PciAddress address;
    if (host_.gpu_pci_address(gpu_index, address) != Status::ok) return;
    uint32_t vendor = 0u;
    uint32_t device = 0u;
    uint32_t revision = 0u;
    if (!read_attribute(address, "vendor", id_limit, vendor) || !read_attribute(address, "device", id_limit, device) ||
        !read_attribute(address, "revision", revision_limit, revision))
        return;
    vendor_id_ = vendor;
    device_id_ = device;
    revision_ = revision;
    present_ = true;
    report(address);
}

bool HostPciIdentity::presented(RvkPciIdentity &identity) const
{
    AV_SCOPE("HostPciIdentity::presented");
    if (!present_) return false;
    identity.vendor_id = static_cast<uint16_t>(vendor_id_);
    identity.device_id = static_cast<uint16_t>(device_id_);
    identity.revision = static_cast<uint8_t>(revision_);
    return true;
}

bool HostPciIdentity::read_attribute(const PciAddress &address, const char *attribute, uint32_t limit,
                                     uint32_t &value)
{
    AV_SCOPE("HostPciIdentity::read_attribute");
    char       path_text[path_bytes] = {};
    TextBuffer path(path_text, sizeof path_text);
    path.append(sysfs_devices);
    address.append_to(path);
    path.append("/");
    path.append(attribute);
    char   text[attribute_bytes] = {};
    size_t length = 0u;
    if (host_.read_file(path.text(), text, sizeof text, length) != Status::ok) return false;
    return parse(text, length, limit, value);
}

bool HostPciIdentity::parse(const char *text, size_t length, uint32_t limit, uint32_t &value) const
{
    AV_SCOPE("HostPciIdentity::parse");
    if (length < 3u || text[0] != '0' || (text[1] != 'x' && text[1] != 'X')) return false;
    uint32_t parsed = 0u;
    size_t   position = 2u;
    for (; position < length; position++) {
        const char c = text[position];
        const bool decimal = c >= '0' && c <= '9';
        const bool lower = c >= 'a' && c <= 'f';
        if (!decimal && !lower && !(c >= 'A' && c <= 'F')) break;
        parsed = parsed * 16u + static_cast<uint32_t>(decimal ? c - '0' : (lower ? c - 'a' : c - 'A') + 10);
        if (parsed > limit) return false;
    }
    if (position == 2u) return false;
    for (; position < length; position++)
        if (text[position] != '\n' && text[position] != ' ' && text[position] != '\t') return false;
    value = parsed;
    return true;
}

void HostPciIdentity::report(const PciAddress &address)
{
    AV_SCOPE("HostPciIdentity::report");
    char       bytes[message_bytes] = {};
    TextBuffer message(bytes, sizeof bytes);
    message.append("virtio-gpu-rvk: pci identity ");
    message.append_lower_hex(vendor_id_, 4u);
    message.append(":");
    message.append_lower_hex(device_id_, 4u);
    message.append(" rev ");
    message.append_lower_hex(revision_, 2u);
    message.append(", the host's ");
    address.append_to(message);
    host_.log(message.text());
}

}  // namespace av
