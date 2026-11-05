// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m av_codegen.d3d --xml tools/codegen/registry/glmem.toml --out ...

#include "proto/generated/glmem/glmem_command_names.h"

#include "proto/generated/glmem/glmem_protocol_ids.h"

namespace av::glmemproto {

const char* command_name(uint32_t id) {
    switch (id) {
        case kCmd_QueryBufferSlicesGL: return "QueryBufferSlicesGL";
        case kCmd_PlaceBufferSlice: return "PlaceBufferSlice";
        case kCmd_ForgetBufferSlice: return "ForgetBufferSlice";
        default: return nullptr;
    }
}

}  // namespace av::glmemproto
