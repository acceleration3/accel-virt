/* The virtio-gpu control commands that choose what a display head shows:
 * SET_SCANOUT_BLOB, and the RESOURCE_FLUSH that names a blob.
 *
 * SET_SCANOUT naming a blob is not here. The base class already answers
 * ERR_INVALID_RESOURCE_ID for a resource it does not hold, and a blob is not
 * one of its resources, so intercepting it would add a decoder to say what is
 * already said. Main loop. */
#ifndef AV_SCANOUT_COMMANDS_HPP
#define AV_SCANOUT_COMMANDS_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlobTable;
class ControlRequest;
class DisplayHeads;
class Host;

class ScanoutCommands {
public:
    enum : uint32_t {   /* VIRTIO_GPU_CMD_* */
        resource_flush = 0x0104u,
        set_scanout_blob = 0x010Du,
    };

    ScanoutCommands(Host &host, BlobTable &blobs, DisplayHeads &heads);
    ScanoutCommands(const ScanoutCommands &) = delete;
    ScanoutCommands &operator=(const ScanoutCommands &) = delete;

    /* Whether the core answered `request`; false leaves it to the base class. */
    bool answer(ControlRequest &request, uint32_t type);

private:
    uint32_t show(ControlRequest &request);
    /* The response for what showing a head answered. */
    uint32_t shown_answer(Status status) const;
    /* False for a request the base class owns: a short one, or one naming a
     * resource that is not a blob. */
    bool     flush(ControlRequest &request, uint32_t &response);

    Host         &host_;
    BlobTable    &blobs_;
    DisplayHeads &heads_;
};

}  // namespace av

#endif  // AV_SCANOUT_COMMANDS_HPP
