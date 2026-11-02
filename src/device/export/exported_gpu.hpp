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

    void         attach(ContextPlane &contexts, const uint8_t *gpu_uuid, const ExportGreeting &greeting,
                        HeadPlanes &heads);
    ExportAnswer greet(const ExportRequest &request) const;
    ExportAnswer export_surface(const ExportRequest &request, int &descriptor) const;
    ExportAnswer sync(const ExportRequest &request, int &descriptor) const;
    ExportAnswer export_head(const ExportRequest &request, int &descriptor) const;

private:
    bool exports() const;

    Host          &host_;
    ContextPlane  *contexts_;
    HeadPlanes    *heads_;
    const uint8_t *gpu_uuid_;
    ExportGreeting greeting_;
};

}

#endif
