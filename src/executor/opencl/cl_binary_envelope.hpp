/* What a program binary crosses to the guest in: the host driver's own bytes,
 * behind a header saying which GPU and which driver made them.
 *
 * An application keeps CL_PROGRAM_BINARIES on disk and hands them back to
 * clCreateProgramWithBinary in a later run (DaVinci Resolve and Affinity do),
 * and what it kept may be from another GPU, another driver version or another
 * host altogether - the guest moved, the host's Mesa was updated. A host
 * driver handed a binary of another's making may refuse it, or may not: rusticl
 * takes a binary with a byte flipped in its middle and says nothing. So the
 * binary is opened only when the header names this device, this driver, every
 * byte of the payload is there and its digest matches; anything else is
 * CL_INVALID_BINARY, which every application that caches binaries answers by
 * building the source again.
 *
 * The header is the host's to read and the guest's to keep: little-endian
 * fields at fixed places, version 1 (docs/protocol.md, "OpenCL program
 * binaries"). */
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
    /* The magic, the version and a reserved word, the two UUIDs, the driver's
     * digest, the payload's length and the payload's digest. */
    static constexpr size_t   header_bytes = 8u + 4u + 4u + 2u * uuid_bytes + 8u + 8u + 8u;
    static constexpr uint32_t version = 1u;

    ClBinaryEnvelope();
    ClBinaryEnvelope(const ClBinaryEnvelope &) = delete;
    ClBinaryEnvelope &operator=(const ClBinaryEnvelope &) = delete;

    /* Learns what a binary of `device` on `platform` is stamped with: the
     * device's and the driver's UUIDs (cl_khr_device_uuid) and the platform's,
     * device's and driver's names and versions. False, knowing nothing, for a
     * device that will not say - which seals and opens nothing. */
    [[nodiscard]] bool identify(cl_platform_id platform, cl_device_id device);
    bool               identified() const;

    /* What a host binary of `host_bytes` takes sealed: the header and the
     * bytes, or nothing for nothing - a device the program has no binary for. */
    size_t sealed_bytes(size_t host_bytes) const;
    /* The header and then `host_bytes` of `host`, into `into`, which has room
     * for sealed_bytes(host_bytes). */
    void   seal(const uint8_t *host, size_t host_bytes, uint8_t *into) const;
    /* The host's own bytes inside `sealed`, when it is a binary this device
     * and driver made, whole; false otherwise, `host` left alone. */
    [[nodiscard]] bool open(const uint8_t *sealed, size_t sealed_size, const uint8_t *&host,
                            size_t &host_bytes) const;

private:
    friend struct ClBinaryEnvelopeProbe;

    /* Longer than any name or version string a driver answers. */
    static constexpr size_t text_bytes = 1024u;
    static constexpr char   magic[9] = "avgpu-cl";

    uint8_t  device_uuid_[uuid_bytes];
    uint8_t  driver_uuid_[uuid_bytes];
    uint64_t driver_digest_;
    bool     identified_;
};

}  // namespace av

#endif  // AV_EXECUTOR_CL_BINARY_ENVELOPE_HPP
