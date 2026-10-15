#include "icd/opencl/cl_destructor_callbacks.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClDestructorCallbacks::Due::Due() : rows_{}, count_(0u)
{
    AV_SCOPE("ClDestructorCallbacks::Due::Due");
}

void ClDestructorCallbacks::Due::run() const
{
    AV_SCOPE("ClDestructorCallbacks::Due::run");
    for (uint32_t index = 0u; index < count_; ++index) {
        const Row &row = rows_[index];
        if (row.mem_ != nullptr)
            row.mem_(static_cast<cl_mem>(const_cast<void *>(row.object_)), row.user_data_);
        else
            row.context_(static_cast<cl_context>(const_cast<void *>(row.object_)), row.user_data_);
    }
}

uint32_t ClDestructorCallbacks::Due::count() const
{
    AV_SCOPE("ClDestructorCallbacks::Due::count");
    return count_;
}

ClDestructorCallbacks::ClDestructorCallbacks() : rows_{}, count_(0u)
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
    if (count_ == max_rows) return Status::no_memory;
    rows_[count_++] = row;
    return Status::ok;
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
    /* Object by object in the order they went - a buffer before the context
     * it held - and each object's latest registered first. */
    for (uint32_t object = 0u; object < count; ++object) {
        for (uint32_t index = count_; index > 0u; --index) {
            if (rows_[index - 1u].object_ == gone[object]) due.rows_[due.count_++] = rows_[index - 1u];
        }
    }
    uint32_t kept = 0u;
    for (uint32_t index = 0u; index < count_; ++index) {
        if (!names_gone(rows_[index], gone, count)) rows_[kept++] = rows_[index];
    }
    count_ = kept;
}

uint32_t ClDestructorCallbacks::kept() const
{
    AV_SCOPE("ClDestructorCallbacks::kept");
    return count_;
}

bool ClDestructorCallbacks::invariants_hold() const
{
    AV_SCOPE("ClDestructorCallbacks::invariants_hold");
    if (count_ > max_rows) return false;
    for (uint32_t index = 0u; index < count_; ++index) {
        const Row &row = rows_[index];
        /* Exactly one callback, of the kind its registration named. */
        if (row.object_ == nullptr || (row.mem_ == nullptr) == (row.context_ == nullptr)) return false;
    }
    return true;
}

}  // namespace av::clicd
