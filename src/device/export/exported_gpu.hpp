/* The GPU a device's export socket hands surfaces of (docs/external-export.md):
 * where the contexts run and the UUID the executor knows the GPU by, and what
 * hello answers. Attached at realize; until then, and wherever the contexts
 * export nothing, a request is answered as by a device that exports nothing.
 * What the executor answers is checked before it is passed on: a descriptor
 * with no picture behind it is closed, never handed over. Its calls run on
 * the connections' threads. */
#ifndef AV_EXPORTED_GPU_HPP
#define AV_EXPORTED_GPU_HPP

#include "proto/export/export_answer.hpp"
#include "proto/export/export_greeting.hpp"

#include <stdint.h>

namespace av {

class ContextPlane;
class HeadPlanes;
class ExportRequest;
class Host;

class ExportedGpu {
public:
    explicit ExportedGpu(Host &host);
    ExportedGpu(const ExportedGpu &) = delete;
    ExportedGpu &operator=(const ExportedGpu &) = delete;

    /* Main loop, at realize, before any connection is served: `gpu_uuid`
     * outlives this. */
    void         attach(ContextPlane &contexts, const uint8_t *gpu_uuid, const ExportGreeting &greeting,
                        HeadPlanes &heads);
    ExportAnswer greet(const ExportRequest &request) const;
    /* The answers to an export and a sync, with the dma-buf or the sync_file
     * that goes beside an ok one in `descriptor`, the caller's; -1 with any
     * other. invalid_state where nothing is exported, not_found for a surface
     * the executor does not export. */
    ExportAnswer export_surface(const ExportRequest &request, int &descriptor) const;
    ExportAnswer sync(const ExportRequest &request, int &descriptor) const;
    /* The answer to a head request: the head's planes composed now into one
     * image (HeadPlanes, ContextPlane::compose_planes), with its dma-buf in
     * `descriptor`, the caller's. not_found for a head with no plane, or one
     * the executor cannot compose. */
    ExportAnswer export_head(const ExportRequest &request, int &descriptor) const;

private:
    bool exports() const;

    Host          &host_;
    ContextPlane  *contexts_;
    HeadPlanes    *heads_;
    const uint8_t *gpu_uuid_;
    ExportGreeting greeting_;
};

}  // namespace av

#endif  // AV_EXPORTED_GPU_HPP
