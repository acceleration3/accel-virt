#include "icd/opencl/cl_destructor_callbacks.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClDestructorCallbacks::Due::Due(BlockSource &memory) : rows_(memory, Pool::paged), lost_(0u)
{
    AV_SCOPE("ClDestructorCallbacks::Due::Due");
}

void ClDestructorCallbacks::Due::run() const
{
    AV_SCOPE("ClDestructorCallbacks::Due::run");
    for (const Row &row : rows_) {
        if (row.mem_ != nullptr)
            row.mem_(static_cast<cl_mem>(const_cast<void *>(row.object_)), row.user_data_);
        else
            row.context_(static_cast<cl_context>(const_cast<void *>(row.object_)), row.user_data_);
    }
}

uint32_t ClDestructorCallbacks::Due::count() const
{
    AV_SCOPE("ClDestructorCallbacks::Due::count");
    return rows_.size();
}

uint32_t ClDestructorCallbacks::Due::lost() const
{
    AV_SCOPE("ClDestructorCallbacks::Due::lost");
    return lost_;
}

ClDestructorCallbacks::ClDestructorCallbacks(BlockSource &memory) : rows_(memory, Pool::paged)
{
    AV_SCOPE("ClDestructorCallbacks::ClDestructorCallbacks");
}

Status ClDestructorCallbacks::keep(cl_mem object, MemCallback callback, void *user_data)
{
    AV_SCOPE("ClDestructorCallbacks::keep");
    return keep_row(Row{object, callback, nullptr, user_data});
}

Status ClDestructorCallbacks::keep(cl_context object, ContextCallback callback, void *user_data)
{
    AV_SCOPE("ClDestructorCallbacks::keep");
    return keep_row(Row{object, nullptr, callback, user_data});
}

Status ClDestructorCallbacks::keep_row(const Row &row)
{
    AV_SCOPE("ClDestructorCallbacks::keep_row");
    if (row.object_ == nullptr || (row.mem_ == nullptr && row.context_ == nullptr)) return Status::invalid_parameter;
    return rows_.append(row);
}

bool ClDestructorCallbacks::names_gone(const Row &row, const void *const *gone, uint32_t count) const
{
    AV_SCOPE("ClDestructorCallbacks::names_gone");
    for (uint32_t index = 0u; index < count; ++index) {
        if (gone[index] == row.object_) return true;
    }
    return false;
}

void ClDestructorCallbacks::take(const void *const *gone, uint32_t count, Due &due)
{
    AV_SCOPE("ClDestructorCallbacks::take");
    for (uint32_t object = 0u; object < count; ++object) {
        for (uint32_t index = rows_.size(); index > 0u; --index) {
            if (rows_.at(index - 1u).object_ != gone[object]) continue;
            if (due.rows_.append(rows_.at(index - 1u)) != Status::ok) due.lost_++;
        }
    }
    uint32_t kept = 0u;
    for (uint32_t index = 0u; index < rows_.size(); ++index) {
        if (!names_gone(rows_.at(index), gone, count)) rows_.at(kept++) = rows_.at(index);
    }
    rows_.truncate(kept);
}

uint32_t ClDestructorCallbacks::kept() const
{
    AV_SCOPE("ClDestructorCallbacks::kept");
    return rows_.size();
}

bool ClDestructorCallbacks::invariants_hold() const
{
    AV_SCOPE("ClDestructorCallbacks::invariants_hold");
    if (!rows_.invariants_hold()) return false;
    for (const Row &row : rows_) {
        if (row.object_ == nullptr || (row.mem_ == nullptr) == (row.context_ == nullptr)) return false;
    }
    return true;
}

}
