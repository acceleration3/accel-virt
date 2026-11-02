#include "um/trace/trace_section.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/trace_layout.hpp"
#include "um/trace/wide_name.hpp"
#include "um/trace/object_security.hpp"

#include <windows.h>

namespace av {

namespace {

constexpr const char *scopes[] = {"Global", "Local"};

}

TraceSection::TraceSection() : mapping_(nullptr), view_(nullptr)
{
    AV_SCOPE("TraceSection::TraceSection");
}

Status TraceSection::create(const char *module, uint32_t pid, ObjectSecurity &security)
{
    AV_SCOPE("TraceSection::create");
    if (view_ != nullptr) return Status::invalid_state;
    const DWORD size = static_cast<DWORD>(bytes());
    for (const char *const scope : scopes) {
        const WideName name(scope, "trace", module, pid);
        HANDLE           mapping = CreateFileMappingW(INVALID_HANDLE_VALUE,
                                                      static_cast<LPSECURITY_ATTRIBUTES>(security.attributes()),
                                                      PAGE_READWRITE, 0u, size, name.text());
        if (mapping == nullptr) continue;
        void *const view = MapViewOfFile(mapping, FILE_MAP_WRITE, 0u, 0u, size);
        if (view == nullptr) {
            CloseHandle(mapping);
            continue;
        }
        mapping_ = mapping;
        view_ = static_cast<uint8_t *>(view);
        return Status::ok;
    }
    return Status::no_memory;
}

uint8_t *TraceSection::region()
{
    AV_SCOPE("TraceSection::region");
    return view_;
}

size_t TraceSection::bytes() const
{
    AV_SCOPE("TraceSection::bytes");
    return TraceLayout(slots).bytes();
}

}
