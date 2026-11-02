#include "icd/opengl/uniform_shapes.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opengl/uniform_shape.hpp"

namespace av::glicd {

UniformShapes::UniformShapes(BlockSource &memory) : values_(memory, Pool::paged)
{
    AV_SCOPE("UniformShapes::UniformShapes");
}

uint64_t UniformShapes::key_of(uint32_t program, int32_t location) const
{
    AV_SCOPE("UniformShapes::key_of");
    return uint64_t{program} << 32u | static_cast<uint32_t>(location);
}

Status UniformShapes::remember(uint32_t program, int32_t location, uint32_t type)
{
    AV_SCOPE("UniformShapes::remember");
    const UniformShape shape(program, location, type);
    if (location < 0 || shape.values() == 0u) return Status::ok;
    uint32_t *const held = values_.find(key_of(program, location));
    if (held != nullptr) {
        *held = shape.values();
        return Status::ok;
    }
    uint32_t slot = 0u;
    return values_.insert(key_of(program, location), shape.values(), slot);
}

void UniformShapes::forget(uint32_t program)
{
    AV_SCOPE("UniformShapes::forget");
    values_.erase_if([program](uint64_t key, uint32_t &) { return key >> 32u == program; });
}

uint32_t UniformShapes::values(uint32_t program, int32_t location) const
{
    AV_SCOPE("UniformShapes::values");
    const uint32_t *const held = values_.find(key_of(program, location));
    return held != nullptr ? *held : 0u;
}

uint32_t UniformShapes::held() const
{
    AV_SCOPE("UniformShapes::held");
    return values_.count();
}

bool UniformShapes::invariants_hold() const
{
    AV_SCOPE("UniformShapes::invariants_hold");
    bool shaped = true;
    values_.for_each([&shaped](uint64_t key, const uint32_t &values) {
        shaped = shaped && static_cast<int32_t>(static_cast<uint32_t>(key)) >= 0 && values != 0u;
    });
    return shaped && values_.invariants_hold();
}

}
