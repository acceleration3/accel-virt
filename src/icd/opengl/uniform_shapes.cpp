#include "icd/opengl/uniform_shapes.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

UniformShapes::UniformShapes() : shapes_{}, count_(0u)
{
    AV_SCOPE("UniformShapes::UniformShapes");
}

void UniformShapes::remember(uint32_t program, int32_t location, uint32_t type)
{
    AV_SCOPE("UniformShapes::remember");
    const UniformShape shape(program, location, type);
    if (location < 0 || shape.values() == 0u) return;
    const uint32_t row = row_of(program, location);
    if (row != capacity) {
        shapes_[row] = shape;
        return;
    }
    if (count_ == capacity) return;
    shapes_[count_++] = shape;
}

void UniformShapes::forget(uint32_t program)
{
    AV_SCOPE("UniformShapes::forget");
    uint32_t kept = 0u;
    for (uint32_t row = 0u; row < count_; row++)
        if (shapes_[row].program() != program) shapes_[kept++] = shapes_[row];
    count_ = kept;
}

uint32_t UniformShapes::values(uint32_t program, int32_t location) const
{
    AV_SCOPE("UniformShapes::values");
    const uint32_t row = row_of(program, location);
    return row == capacity ? 0u : shapes_[row].values();
}

uint32_t UniformShapes::held() const
{
    AV_SCOPE("UniformShapes::held");
    return count_;
}

bool UniformShapes::invariants_hold() const
{
    AV_SCOPE("UniformShapes::invariants_hold");
    if (count_ > capacity) return false;
    for (uint32_t row = 0u; row < count_; row++) {
        if (shapes_[row].location() < 0 || shapes_[row].values() == 0u) return false;
        for (uint32_t earlier = 0u; earlier < row; earlier++)
            if (shapes_[earlier].program() == shapes_[row].program() &&
                shapes_[earlier].location() == shapes_[row].location())
                return false;
    }
    return true;
}

uint32_t UniformShapes::row_of(uint32_t program, int32_t location) const
{
    AV_SCOPE("UniformShapes::row_of");
    for (uint32_t row = 0u; row < count_; row++)
        if (shapes_[row].program() == program && shapes_[row].location() == location) return row;
    return capacity;
}

}  // namespace av::glicd
