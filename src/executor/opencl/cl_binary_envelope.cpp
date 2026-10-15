#include "executor/opencl/cl_binary_envelope.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/opencl/cl_binary_digest.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <CL/cl_ext.h>

namespace av {

ClBinaryEnvelope::ClBinaryEnvelope() : device_uuid_{}, driver_uuid_{}, driver_digest_(0u), identified_(false)
{
    AV_SCOPE("ClBinaryEnvelope::ClBinaryEnvelope");
}

bool ClBinaryEnvelope::identify(cl_platform_id platform, cl_device_id device)
{
    AV_SCOPE("ClBinaryEnvelope::identify");
    identified_ = false;
    if (platform == nullptr || device == nullptr) return false;
    if (clGetDeviceInfo(device, CL_DEVICE_UUID_KHR, uuid_bytes, device_uuid_, nullptr) != CL_SUCCESS ||
        clGetDeviceInfo(device, CL_DRIVER_UUID_KHR, uuid_bytes, driver_uuid_, nullptr) != CL_SUCCESS)
        return false;
    /* The UUIDs say which GPU and which build of its driver; the names and
     * versions are in the digest too, for a driver whose UUID outlives a
     * change of its compiler. */
    static constexpr cl_platform_info platform_texts[] = {CL_PLATFORM_NAME, CL_PLATFORM_VERSION};
    static constexpr cl_device_info   device_texts[] = {CL_DEVICE_NAME, CL_DEVICE_VERSION, CL_DRIVER_VERSION};
    ClBinaryDigest digest;
    char           text[text_bytes] = {};
    for (const cl_platform_info name : platform_texts) {
        if (clGetPlatformInfo(platform, name, sizeof text - 1u, text, nullptr) != CL_SUCCESS) return false;
        text[sizeof text - 1u] = '\0';
        digest.add_text(text);
    }
    for (const cl_device_info name : device_texts) {
        if (clGetDeviceInfo(device, name, sizeof text - 1u, text, nullptr) != CL_SUCCESS) return false;
        text[sizeof text - 1u] = '\0';
        digest.add_text(text);
    }
    driver_digest_ = digest.value();
    identified_ = true;
    return true;
}

bool ClBinaryEnvelope::identified() const
{
    AV_SCOPE("ClBinaryEnvelope::identified");
    return identified_;
}

size_t ClBinaryEnvelope::sealed_bytes(size_t host_bytes) const
{
    AV_SCOPE("ClBinaryEnvelope::sealed_bytes");
    return host_bytes != 0u ? header_bytes + host_bytes : 0u;
}

void ClBinaryEnvelope::seal(const uint8_t *host, size_t host_bytes, uint8_t *into) const
{
    AV_SCOPE("ClBinaryEnvelope::seal");
    if (host == nullptr || host_bytes == 0u || into == nullptr) return;
    ClBinaryDigest payload;
    payload.add(host, host_bytes);
    WireWriter header(into, header_bytes);
    header.write(magic, sizeof magic - 1u);
    header.write_u32(version);
    header.write_u32(0u);
    header.write(device_uuid_, uuid_bytes);
    header.write(driver_uuid_, uuid_bytes);
    header.write_u64(driver_digest_);
    header.write_u64(host_bytes);
    header.write_u64(payload.value());
    const uint8_t *const from = host;
    uint8_t *const       to = into + header_bytes;
    for (size_t index = 0u; index < host_bytes; ++index) to[index] = from[index];
}

bool ClBinaryEnvelope::open(const uint8_t *sealed, size_t sealed_size, const uint8_t *&host,
                            size_t &host_bytes) const
{
    AV_SCOPE("ClBinaryEnvelope::open");
    if (!identified_ || sealed == nullptr || sealed_size <= header_bytes) return false;
    WireReader header(sealed, header_bytes);
    uint8_t    read_magic[sizeof magic - 1u] = {};
    uint8_t    read_device[uuid_bytes] = {};
    uint8_t    read_driver[uuid_bytes] = {};
    if (!header.read(read_magic, sizeof read_magic)) return false;
    const uint32_t read_version = header.read_u32();
    const uint32_t reserved = header.read_u32();
    if (!header.read(read_device, uuid_bytes) || !header.read(read_driver, uuid_bytes)) return false;
    const uint64_t read_driver_digest = header.read_u64();
    const uint64_t payload_bytes = header.read_u64();
    const uint64_t payload_digest = header.read_u64();
    if (header.status() != Status::ok) return false;
    bool same = read_version == version && reserved == 0u && read_driver_digest == driver_digest_;
    for (size_t index = 0u; index < sizeof read_magic; ++index)
        same = same && read_magic[index] == static_cast<uint8_t>(magic[index]);
    for (uint32_t index = 0u; index < uuid_bytes; ++index) {
        same = same && read_device[index] == device_uuid_[index] && read_driver[index] == driver_uuid_[index];
    }
    /* Every byte the header promised, and nothing after it: a binary cut
     * short or run on is not the one that was sealed. */
    if (!same || payload_bytes != sealed_size - header_bytes) return false;
    ClBinaryDigest payload;
    payload.add(sealed + header_bytes, static_cast<size_t>(payload_bytes));
    if (payload.value() != payload_digest) return false;
    host = sealed + header_bytes;
    host_bytes = static_cast<size_t>(payload_bytes);
    return true;
}

}  // namespace av
