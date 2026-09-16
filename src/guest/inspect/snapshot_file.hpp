/* One file rvkinspect.exe answers with (docs/trace.md): `RVKSNAP1`, the clock
 * when the copy was taken, the kind and the payload's length, then the
 * payload. */
#ifndef AV_SNAPSHOT_FILE_HPP
#define AV_SNAPSHOT_FILE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class InspectHost;

class SnapshotFile {
public:
    static constexpr size_t header_bytes = 24u;

    enum class Kind : uint32_t { table = 1u, state = 2u, error = 3u };

    SnapshotFile(InspectHost &host, const char *path);
    SnapshotFile(const SnapshotFile &) = delete;
    SnapshotFile &operator=(const SnapshotFile &) = delete;

    /* too_large for a payload a u32 cannot count, with nothing written;
     * otherwise the host's answer. */
    [[nodiscard]] Status write(Kind kind, uint64_t clock, const uint8_t *payload, size_t length);
    /* An error file whose payload is `text`. */
    [[nodiscard]] Status refuse(uint64_t clock, const char *text);

private:
    InspectHost &host_;
    const char  *path_;
};

}  // namespace av

#endif  // AV_SNAPSHOT_FILE_HPP
