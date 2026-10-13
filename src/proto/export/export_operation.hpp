/* What a message on a device's export socket asks or tells
 * (docs/external-export.md): the consumer's six requests, and the one event
 * the device sends unasked. */
#ifndef AV_EXPORT_OPERATION_HPP
#define AV_EXPORT_OPERATION_HPP

#include <stdint.h>

namespace av {

enum class ExportOperation : uint32_t {
    /* Device -> consumer only: the answer to a request it could not read. */
    unreadable = 0u,
    hello = 1u,
    export_surface = 2u,
    sync = 3u,
    release = 4u,
    /* The consumer's display: its refresh, and when it blanks. */
    timing = 5u,
    /* What a display head shows, its overlay planes composed now: the head
     * is the surface field less one (docs/external-export.md). */
    head = 6u,
    /* Device -> consumer only: a surface it exported is gone. */
    revoked = 0x80u
};

/* Why a surface was revoked. */
enum class ExportRevocation : uint32_t {
    released = 1u,   /* the guest let go of it everywhere */
    reset = 2u       /* the device was reset: every surface, under id 0 */
};

}  // namespace av

#endif  // AV_EXPORT_OPERATION_HPP
