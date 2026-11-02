#include "guest/kmt/beam_position.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

BeamPosition::BeamPosition() : in_vertical_blank_(false), line_(0u)
{
    AV_SCOPE("BeamPosition::BeamPosition");
}

BeamPosition::BeamPosition(bool in_vertical_blank, uint32_t line) : in_vertical_blank_(in_vertical_blank), line_(line)
{
    AV_SCOPE("BeamPosition::BeamPosition");
}

bool BeamPosition::earlier_than(const BeamPosition &other) const
{
    AV_SCOPE("BeamPosition::earlier_than");
    if (in_vertical_blank_ != other.in_vertical_blank_) return in_vertical_blank_;
    return line_ < other.line_;
}

}
