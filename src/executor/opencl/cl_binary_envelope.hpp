#ifndef AV_EXECUTOR_CL_BINARY_ENVELOPE_HPP
#define AV_EXECUTOR_CL_BINARY_ENVELOPE_HPP

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av {

struct ClBinaryEnvelopeProbe;

class ClBinaryEnvelope {
public:
    static constexpr uint32_t uuid_bytes = 16u;
    static constexpr size_t   header_bytes = 8u + 4u + 4u + 2u * uuid_bytes + 8u + 8u + 8u;
    static constexpr uint32_t version = 1u;

    ClBinaryEnvelope();
    ClBinaryEnvelope(const ClBinaryEnvelope &) = delete;
    ClBinaryEnvelope &operator=(const ClBinaryEnvelope &) = delete;

    [[nodiscard]] bool identify(cl_platform_id platform, cl_device_id device);
    bool               identified() const;

    size_t sealed_bytes(size_t host_bytes) const;
    void   seal(const uint8_t *host, size_t host_bytes, uint8_t *into) const;
    [[nodiscard]] bool open(const uint8_t *sealed, size_t sealed_size, const uint8_t *&host,
                            size_t &host_bytes) const;

private:
    friend struct ClBinaryEnvelopeProbe;

    static constexpr size_t text_bytes = 1024u;
    static constexpr char   magic[9] = "avgpu-cl";

    uint8_t  device_uuid_[uuid_bytes];
    uint8_t  driver_uuid_[uuid_bytes];
    uint64_t driver_digest_;
    bool     identified_;
};

}

#endif
