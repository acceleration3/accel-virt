/* One command inside a recorded batch: its header, the payload that follows it
 * within the record, and the name the generator gave its id. The bytes are the
 * guest's, so the framing is judged against the record's own length and
 * nothing is read past it. */
#ifndef AV_RECORDED_COMMAND_HPP
#define AV_RECORDED_COMMAND_HPP

#include "core/support/status.hpp"
#include "proto/submission/command_header.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class RecordedCommand {
public:
    RecordedCommand();

    /* The command at `offset` of the `length` bytes at `bytes`: malformed for
     * a header the batch cannot hold, or a size that passes its end. Unchanged
     * unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, uint32_t length, uint32_t offset);

    const CommandHeader &header() const;
    /* The bytes after the command's own header. */
    const uint8_t       *payload() const;
    uint32_t             payload_length() const;
    /* Where this command starts in the batch, and where the next one would. */
    uint32_t             offset() const;
    uint32_t             next_offset() const;
    /* The name the registry gave this id, or the transport command's own;
     * "unknown" for an id no table knows, never null. */
    const char          *name() const;
    /* The API's name: transport, vulkan, opencl, opengl, or "api". */
    const char          *api_name() const;

private:
    static constexpr uint16_t vulkan_api = 1u;
    static constexpr uint16_t opencl_api = 2u;
    static constexpr uint16_t opengl_api = 3u;

    CommandHeader  header_;
    const uint8_t *payload_;
    uint32_t       payload_length_;
    uint32_t       offset_;
};

}  // namespace av

#endif  // AV_RECORDED_COMMAND_HPP
